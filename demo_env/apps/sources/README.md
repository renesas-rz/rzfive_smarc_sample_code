# lws minimal ws server (threads)

## build

```
 $ mkdir build
 $ cd build
 $ cmake .. # Releaes build
 $ cmake -DCMAKE_BUILD_TYPE=Debug .. # Debug build
 $ make
```

Pthreads and jansson is required on your system.

## usage

```
 $ ./lws-minimal-ws-server-threads
[2018/03/13 13:09:52:2208] USER: LWS minimal ws server + threads | visit http://localhost:3000
[2018/03/13 13:09:52:2365] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
```

Visit http://localhost:3000 on single browser windows

Sensor thread get sensor data and add them to a ringbuffer,
signalling lws to send new entries to the browser window.

When the broser window send led control message to lws, lws add led state to another ringbuffer,
led thread wake up and get led state and manipulate led GPIO.
