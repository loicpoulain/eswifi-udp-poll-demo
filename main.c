#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udp_poll_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/udp.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/sys/eventfd.h>

#include <stddef.h>
#include <errno.h>

/**
 * Configure remote IP, scenario, and poll timeout in config.h
 */
#include "config.h"

#define SOCKET_FD_IDX 0
#define EVENT_FD_IDX 1

/* These manage what gets set up and what gets passed to poll() */
#if SOCKET_ONLY
  #define EVENT_OFFSET 0
  #define EVENT_COUNT 1
  #define EVENTFD_ONLY 0
#elif EVENTFD_ONLY
  #define EVENT_OFFSET 1
  #define EVENT_COUNT 1
  #define SOCKET_ONLY 0
#elif SOCKET_AND_EVENTFD
  #define EVENT_OFFSET 0
  #define EVENT_COUNT 2
  #define EVENTFD_ONLY 0
  #define SOCKET_ONLY 0
#endif

int event_fd;

/**
 * Writes an incrementing value to the current eventfd
*/
static void my_expiry_function(struct k_timer *timer_id) {
  static int count = 1;
  LOG_INF("Writing (%d) event_fd %d...", count, event_fd);
  eventfd_write(event_fd, count++);
}

K_TIMER_DEFINE(my_timer, my_expiry_function, NULL);

/**
 * Create and connect to a UDP socket
*/
static int _connect(char *ipaddr, uint32_t port) {
  int sock;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ipaddr, &addr.sin_addr);

  LOG_INF("Connecting to %s:%d", ipaddr, port);

  sock = zsock_socket(addr.sin_family, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    LOG_ERR("Failed to create socket: %d", errno);
    return -errno;
  }

  if (zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_ERR("Failed to set socket peer address %s: %d", ipaddr, errno);
    return -errno;
  }

  return sock;
}

/**
 * Poll the event source(s)
*/
static int _poll(int sock, struct zsock_pollfd *pollfds) {
  int ret;
  int events;
  uint8_t rx_buf[256];

  events = zsock_poll(pollfds + EVENT_OFFSET, EVENT_COUNT, POLL_TIMEOUT);
  if (events <= 0) {

    switch (errno) {
      case ETIMEDOUT:
        LOG_ERR("Poll error: unexpectedly timed out");
        break;
      case EINVAL:
        LOG_ERR("Poll error: invalid argument. %d event sources. connected to wifi?", EVENT_COUNT);
        break;
      default:
        LOG_ERR("Poll error: %d", errno);
    }

    return -errno;
  }

#if (EVENTFD_ONLY || SOCKET_AND_EVENTFD)

  if (pollfds[EVENT_FD_IDX].revents & ZSOCK_POLLIN) {
    uint64_t event;

    ret = eventfd_read(pollfds[EVENT_FD_IDX].fd, &event);
    if (ret < 0) {
      LOG_ERR("Error reading event: %d", ret);
      return ret;
    }
    LOG_INF("Received eventfd event, value: %lld", event);
  }

#endif

#if (SOCKET_ONLY || SOCKET_AND_EVENTFD)

  if (pollfds[SOCKET_FD_IDX].revents & ZSOCK_POLLIN) {
    LOG_DBG("Received socket event: %d",
            pollfds[SOCKET_FD_IDX].revents);
  }

  if (pollfds[SOCKET_FD_IDX].revents & ZSOCK_POLLNVAL) {
    LOG_ERR("Invalid socket file descriptor.");
    return -EBADF;
  }

  if (pollfds[SOCKET_FD_IDX].revents & ZSOCK_POLLERR) {
    LOG_ERR("Unexpected socket polling error");
    zsock_close(sock);
    sock = -1;
    return -ECONNABORTED;
  }

  if (pollfds[SOCKET_FD_IDX].revents & ZSOCK_POLLHUP) {
    LOG_ERR("Unexpected socket hangup.");
    zsock_close(sock);
    sock = -1;
    return -ECONNRESET;
  }
  
  memset(rx_buf, 0, sizeof(rx_buf)); 
  ret = zsock_recvfrom(sock, rx_buf, sizeof(rx_buf), ZSOCK_MSG_TRUNC, NULL, NULL);
  if (ret > sizeof(rx_buf)) {
    LOG_ERR("Buffer size insufficient to receive data packet");
    return -ENOBUFS;
  }
  
  LOG_INF("Received: %s", rx_buf);

#endif

  return 0;
}

void main() {
  int ret;
  int sock;
  struct zsock_pollfd pollfds[2];

  if (SOCKET_AND_EVENTFD) {
    LOG_INF("Scenario: polling eventfd, periodic notification, AND udp socket, connecting to: %s:%d, indefinite timeout (-1)", REMOTE_IP, PORT);
    LOG_INF("Expectation: wait indefinitely for eventfd to be signaled or incoming data on socket");
    LOG_INF("Actual: invalid argument because of 2 event sources passed to poll");
  } else if (SOCKET_ONLY) {
    LOG_INF("Scenario: polling single udp socket, connecting to  %s:%d, indefinite timeout (-1)", REMOTE_IP, PORT);
    LOG_INF("Expectation: wait indefinitely for incoming data on socket");
    LOG_INF("Actual: immediate timeout");
  } else if (EVENTFD_ONLY) {
    LOG_INF("Scenario: polling single eventfd, periodic notification, indefinite timeout (-1)");
    LOG_INF("Expectation: wait indefinitely for eventfd to be signaled");
    LOG_INF("Actual: waits indefinitely for eventfd to be signaled");
  }
  
  LOG_INF("---------------");

#if (SOCKET_ONLY || SOCKET_AND_EVENTFD)
  LOG_INF("Waiting 10 seconds for wifi to come up...");
  k_msleep(10000);

  sock = _connect(REMOTE_IP, PORT);
  if (sock < 0) {
    LOG_ERR("Failed to connect: %d", sock);
    return;
  }
  LOG_INF("Socket fd: %d", sock);
  int r = zsock_send(sock, "Hello!\n", 7, 0);
  if (r < 0) {
    LOG_WRN("Failed to send. Proceeding to poll anyway.");
  }
#else
  sock = -1;
#endif

#if (EVENTFD_ONLY || SOCKET_AND_EVENTFD)
  k_timer_start(&my_timer, K_SECONDS(7), K_SECONDS(7));
  
  event_fd = eventfd(0, 0);
  LOG_INF("Eventfd: %d", event_fd);
#else
  event_fd = -1;
#endif

  pollfds[SOCKET_FD_IDX].fd = sock;
  pollfds[SOCKET_FD_IDX].events = ZSOCK_POLLIN;
  pollfds[EVENT_FD_IDX].fd = event_fd;
  pollfds[EVENT_FD_IDX].events = ZSOCK_POLLIN;

  while(true) {
    ret = _poll(sock, pollfds);
    if (ret < 0) {
      LOG_INF("Sleeping before next poll");
      k_msleep(5000);
    }
  }
}
