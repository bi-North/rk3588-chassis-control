# Chassis Daemon

`chassis_daemon` is the first long-running Linux chassis service. It keeps the
100 Hz motor control loop alive and receives normalized velocity commands over
UDP.

## Safety Behavior

- The daemon waits until all four motors report feedback.
- If any motor feedback times out, all current commands are set to zero.
- If no new velocity command arrives for 300 ms, the chassis stops.
- A zero velocity command resets the speed PID state and sends zero current.
  The chassis does not actively resist manual movement while stopped.
- When the daemon exits, it sends zero current repeatedly.
- The default UDP bind address is `127.0.0.1`, so only local programs can
  control the chassis during the first validation.

## Build

```bash
cd ~/projects/rk3588-chassis-control && git pull && cd build && cmake --build . --clean-first
```

## Start The Daemon

Open terminal A:

```bash
cd ~/projects/rk3588-chassis-control/build
./chassis_daemon can0
```

Expected startup output:

```text
Waiting for all motor feedback...
chassis_daemon listening on udp://127.0.0.1:20001 can=can0 max_translate=500 max_rotate=400 timeout=300ms
```

The daemon prints the latest command, command age, wheel target rpm, actual rpm,
and current command.

## Send Commands

Open terminal B:

```bash
cd ~/projects/rk3588-chassis-control/build
```

Forward:

```bash
python3 ../tools/send_velocity.py 0.8 0.0 0.0 --duration-ms 3000
```

Right strafe:

```bash
python3 ../tools/send_velocity.py 0.0 0.8 0.0 --duration-ms 3000
```

Left rotation:

```bash
python3 ../tools/send_velocity.py 0.0 0.0 0.3 --duration-ms 3000
```

Combined motion:

```bash
python3 ../tools/send_velocity.py 0.3 0.2 0.0 --duration-ms 3000
```

## Stop

Press `Ctrl+C` in terminal A. The daemon sends zero current before exiting.

## UDP Protocol

The UDP payload is one line with three floating-point values:

```text
<forward> <strafe> <rotate>
```

Example:

```text
0.4000 0.0000 0.0000
```

Each value is clamped to the range `-1.0` to `1.0`.

## Future Integration

The UDP protocol is intentionally small. Later work can connect:

- A Linux keyboard or joystick process
- A ROS2 `/cmd_vel` bridge
- A Linux to RT-Thread proxy

The daemon remains responsible for SocketCAN, 3508 feedback handling, speed
PID, command timeout, and zero-current shutdown.
