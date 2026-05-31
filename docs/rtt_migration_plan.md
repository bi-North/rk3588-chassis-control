# RT-Thread Migration Plan

The Linux chassis implementation is now stable enough to serve as the reference
behavior for the RT-Thread migration.

## Current Linux Reference

Validated Linux behavior:

- USB-CAN appears as SocketCAN `can0`
- CAN bitrate is `1000000`
- DJI 3508 feedback IDs are `0x201` to `0x204`
- Current command ID is `0x200`
- The chassis control period is 10 ms
- Forward, backward, strafe, and rotation directions are verified
- Feedback timeout, command timeout, and zero-current shutdown are verified
- `rk3588-can.service` and `rk3588-chassis.service` start correctly after reboot

## Code That Can Move To RT-Thread

Portable control logic:

```text
src/pid.c
include/pid.h
src/chassis_control.c
include/chassis_control.h
src/motor_3508.c
include/motor_3508.h
```

These files contain:

- PID calculation
- Four-wheel mecanum kinematics
- 3508 frame packing and feedback parsing
- Command timeout
- Feedback timeout
- PID reset on zero command

## Code That Stays On Linux

Linux-specific components:

```text
src/socketcan.c
include/socketcan.h
tools/chassis_daemon.c
scripts/setup_can.sh
systemd/*
```

These files contain:

- SocketCAN access
- USB-CAN device configuration
- Linux systemd startup
- Linux-side logging and diagnostics

## Code That Must Be Added

Linux:

```text
Linux CAN proxy
Linux RT-Thread UDP bridge
ROS2 /cmd_vel adapter
```

RT-Thread:

```text
UDP or ivshmem-nic endpoint
100 Hz chassis control thread
Linux heartbeat monitor
state packet publisher
emergency stop handler
```

## Migration Sequence

1. Probe the mixed-deployment virtual NIC and confirm the RT-Thread peer.
2. Build an echo test over `ivshmem-nic`.
3. Send velocity commands from Linux to RT-Thread and verify timeout handling.
4. Run PID and mecanum kinematics inside RT-Thread without sending CAN frames.
5. Return computed current commands from RT-Thread to a Linux CAN proxy.
6. Compare Linux reference outputs with RT-Thread outputs.
7. Switch motor control ownership from `chassis_daemon` to RT-Thread.
8. Add ROS2 `/cmd_vel` input on Linux.
9. Run reboot, disconnect, and emergency-stop validation.

## Immediate Next Step

Run the environment probe on RK3588:

```bash
cd ~/projects/rk3588-chassis-control && ./scripts/probe_hybrid_ipc.sh
```

The output determines the exact RT-Thread bridge implementation.
