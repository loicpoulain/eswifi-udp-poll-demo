#ifndef DEMO_CONFIG_H
#define DEMO_CONFIG_H

/**
 * Set to your computer's IP address
 * Use something like netcat to write to the socket
 * $ nc -lu 5555 
 */
#define REMOTE_IP "192.168.1.165"
#define PORT 5555

/**
  * Pick a scenario to demonstrate
  */
#define SOCKET_AND_EVENTFD 1
#define SOCKET_ONLY 0 
#define EVENTFD_ONLY 0

/**
  * Set poll timeout. 
  * -1 for indefinite timeout, otherwise a timeout value in ms
  */
#define POLL_TIMEOUT -1

#endif
