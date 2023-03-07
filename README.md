# Demonstrate eswifi poll bugs

Setup the demo by choosing options in config.h

1. SOCKET_ONLY sets up poll with a single udp socket
2. EVENTFD_ONLY sets up poll with a single eventfd source
3. SOCKET_AND_EVENTFD sets up poll with two event sources, eventfd and socket

You will need to connect to WiFi in order to test the UDP portion, so have your shell command ready to paste in:

```
wifi connect <SSID> <password>
```

Issues:

1. SOCKET_ONLY doesn't wait indefinitely for timeout, and returns immediately
2. EVENTFD_ONLY works, no problems. 
3. SOCKET_AND_EVENTFD doesn't work because poll doesn't accept more than a single event source

* board: b_l4s5i_iot01a
* sdk: 0.15.2
* zephyr 3.3.0
