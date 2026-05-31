# RT-Thread Port Staging Files

This directory contains RT-Thread-side source files that must be integrated
into the actual RT-Thread application project used to build the mixed Linux +
RT-Thread image.

## UDP Echo Module

`rtt_udp_echo.c` is the first application-layer IPC test. It listens on:

```text
udp://0.0.0.0:21001
```

and echoes each received datagram without modifying it.

The module requires the RT-Thread SAL socket interface and an operational
network stack. The successful Linux-side ping to `10.10.10.30` confirms that
the deployed image already has a working virtual NIC and IP stack.

Copy the source file into the existing RT-Thread application and add it to that
project's build sources. After rebuilding and deploying the image, run this
command in the RT-Thread shell:

```text
rtt_udp_echo_start
```

Expected shell output:

```text
rtt_udp_echo: listening on udp/0.0.0.0:21001
```

The exact build-file edit depends on the RT-Thread project layout. Do not guess
the integration location: inspect the real project first.
