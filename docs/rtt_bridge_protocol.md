# Linux To RT-Thread Bridge Protocol

This document defines the proposed first bridge between RK3588 Linux and the
RT-Thread guest.

## Confirmed Platform Facts

The provided HyperBoot guide shows:

- A Linux + RT-Thread mixed deployment image
- Linux kernel modules `ivshmem-nic.ko` and `ivshmem-uio.ko`
- A Linux virtual NIC named `enp255s5`
- A documented Linux-side static IP address `10.10.10.31/24`

The RT-Thread peer IP address and its application API still need to be
confirmed from the running image or the `rockchip-hypercar` RT-Thread source.

## Recommended First Transport

Use UDP over the `ivshmem-nic` virtual interface first.

Reasons:

- It uses the existing virtualization network driver.
- Linux and RT-Thread remain loosely coupled.
- Packets are easy to inspect and log.
- The control protocol can later move to direct shared memory without changing
  the payload semantics.

Direct `ivshmem-uio` shared-memory access is an optimization path, not the first
integration step.

## Target Architecture

```text
Linux ROS2, AI, SLAM, navigation
        |
        | velocity command
        v
Linux RT-Thread bridge
        |
        | UDP over ivshmem-nic
        v
RT-Thread chassis task at 100 Hz
        |
        | target wheel rpm, PID, safety state
        v
Linux CAN proxy
        |
        | SocketCAN can0 through USB-CAN
        v
DJI 3508 ESCs
```

USB-CAN remains attached to Linux because the current virtualized deployment
does not expose CAN directly to RT-Thread.

## Messages

Constants and portable payload structures are defined in:

```text
include/rtt_bridge_protocol.h
```

The wire format uses network byte order. Implementations must serialize fields
explicitly. Do not send C structures by casting them to byte arrays.

Message types:

```text
1  velocity command
2  chassis state
3  heartbeat
4  emergency stop
```

Normalized velocity commands use signed permille values:

```text
forward_permille  -1000 to 1000
strafe_permille   -1000 to 1000
rotate_permille   -1000 to 1000
```

The verified Linux coordinate convention is:

```text
forward > 0  moves forward
strafe  > 0  moves right
rotate  > 0  rotates left, counterclockwise
```

## Safety Rules

- RT-Thread stops the chassis if velocity commands time out.
- Linux CAN proxy sends zero current if RT-Thread state packets time out.
- Emergency stop packets always override motion commands.
- All four motor feedback timestamps must remain valid before current commands
  are applied.
- Linux keeps its existing zero-current shutdown behavior.

## Next Confirmation

Run:

```bash
cd ~/projects/rk3588-chassis-control && ./scripts/probe_hybrid_ipc.sh
```

Send the complete output before implementing the live bridge.
