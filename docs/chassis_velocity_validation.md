# Chassis Velocity Validation

This document records the current RK3588 Linux chassis velocity validation.

## Environment

- Board: RK3588 Linux
- CAN interface: `can0`
- CAN bitrate: `1000000`
- Motor drivers: DJI 3508 ESCs
- Control path: Linux user process to SocketCAN to USB-CAN to 3508 ESCs
- Control period: 10 ms

## Current Result

Basic chassis movement has been manually verified after the forward direction
fix and a clean rebuild.

Command convention:

```text
forward > 0  moves the chassis forward
strafe  > 0  moves the chassis right
rotate  > 0  rotates the chassis left, counterclockwise
```

Validated basic movements:

```text
forward/backward: pass
left/right strafe: pass
left/right rotation: pass
```

## Build Command

Use a clean rebuild after pulling updates:

```bash
cd ~/projects/rk3588-chassis-control && git pull && cd build && cmake --build . --clean-first
```

## Validation Commands

Forward:

```bash
./chassis_velocity can0 0.4 0.0 0.0 3000 500 400
```

Backward:

```bash
./chassis_velocity can0 -0.4 0.0 0.0 3000 500 400
```

Right strafe:

```bash
./chassis_velocity can0 0.0 0.4 0.0 3000 500 400
```

Left strafe:

```bash
./chassis_velocity can0 0.0 -0.4 0.0 3000 500 400
```

Left rotation:

```bash
./chassis_velocity can0 0.0 0.0 0.3 3000 500 400
```

Right rotation:

```bash
./chassis_velocity can0 0.0 0.0 -0.3 3000 500 400
```

Combined forward-right motion:

```bash
./chassis_velocity can0 0.3 0.2 0.0 3000 500 400
```

## Pass Criteria

- The program reports `online=1`.
- The chassis moves in the expected direction.
- The program stops after the requested duration.
- The program sends zero current before exiting.
- No motor feedback timeout occurs during the run.

## Next Engineering Step

The next step is to add a long-running chassis service. The service should:

- Keep the 100 Hz motor control loop alive.
- Receive `forward`, `strafe`, and `rotate` commands from a higher-level
  process.
- Stop automatically if no new command is received within a timeout.
- Keep SocketCAN and 3508 motor protocol details inside the Linux control
  process.

This service will become the Linux-side entry point for ROS2, keyboard/manual
control, or the future RT-Thread bridge.
