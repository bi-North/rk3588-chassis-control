# RT-Thread UDP Echo Test

This is the first application-layer check for the Linux to RT-Thread bridge.
It does not send motor commands.

## Confirmed Network Endpoints

```text
Linux enp255s5  10.10.10.31/24
RT-Thread peer  10.10.10.30
```

Linux can ping `10.10.10.30` through `enp255s5` with zero packet loss.

## Integrate The RT-Thread Module

The staging source is:

```text
rtthread_port/rtt_udp_echo.c
```

Copy it into the actual RT-Thread application project and add it to the build.
The actual project must provide:

```text
RT-Thread MSH shell
SAL socket API
working ivshmem-nic virtual network interface
RT-Thread address 10.10.10.30
```

After rebuilding and deploying the RT-Thread image, open its shell and run:

```text
rtt_udp_echo_start
```

Expected output:

```text
rtt_udp_echo: listening on udp/0.0.0.0:21001
```

## Run The Linux Probe

On RK3588 Linux:

```bash
cd ~/projects/rk3588-chassis-control && git pull && python3 tools/test_rtt_udp_echo.py
```

Expected result:

```text
summary: sent=10 received=10 lost=0
```

Each packet is a serialized bridge heartbeat message, not arbitrary text. A
successful test confirms:

- Linux can send UDP packets to RT-Thread.
- RT-Thread application code can receive packets.
- RT-Thread can send UDP packets back to Linux.
- The first bridge protocol packet crosses the virtual network intact.

## Safety Boundary

Keep `rk3588-chassis.service` running normally during this test. The echo test
uses port `21001` and does not interact with SocketCAN, the chassis daemon port
`20001`, or motor current commands.

## After This Passes

Replace the RT-Thread echo action with protocol parsing, command timeout
handling, and a state response. Then run RT-Thread kinematics and PID in
shadow mode before allowing RT-Thread output to reach the Linux CAN proxy.
