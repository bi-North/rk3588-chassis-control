# RT-Thread CAN Proxy Stage 1

This stage keeps USB-CAN on Linux and moves the chassis control entry toward
RT-Thread through a small UDP bridge.

## Architecture

```text
RT-Thread rtt_chassis_bridge
        |
        | UDP over ivshmem-nic, 10.10.10.30 <-> 10.10.10.31
        v
Linux can_proxy
        |
        | SocketCAN can0
        v
DJI 3508 ESC feedback and current frames
```

Stage 1 is intentionally safe:

- RT-Thread sends heartbeat and zero-current command packets.
- Linux `can_proxy` reads 3508 feedback and sends state packets to RT-Thread.
- Linux `can_proxy` ignores non-zero current commands unless started with
  `--allow-current`.

## Linux Build

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build .
```

Run the proxy:

```bash
sudo systemctl stop rk3588-chassis.service
./can_proxy can0 10.10.10.31 21001
```

Expected output after RT-Thread starts:

```text
rt=10.10.10.30 age=... hb=... online=0xF allow_current=0 M1 rpm=...
```

`online=0xF` means all four 3508 feedback frames are being received.

## RT-Thread Integration

Copy these files into the vendor RT-Thread project:

```text
rtthread_port/rtt_chassis_bridge.c -> applications/rtt_chassis_bridge.c
rtthread_port/main_with_chassis_bridge.c -> applications/main.c
```

Then rebuild `rtthread.bin`, rebuild `HyperBoot.bin`, replace
`/boot/HyperBoot.bin`, and reboot.

## Acceptance

1. `ping -c 4 -I enp255s5 10.10.10.30` succeeds on Linux.
2. `can_proxy` prints RT heartbeat count increasing.
3. `can_proxy` prints `online=0xF` after the 3508 ESCs are powered.
4. RT-Thread log prints state packet count increasing.

Do not use `--allow-current` until this stage passes.
