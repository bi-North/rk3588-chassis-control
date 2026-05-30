# Chassis Velocity Interface

`chassis_velocity` is the command-line entry point closest to the later control
interface. It sends normalized chassis velocity commands to the four 3508
motors through SocketCAN.

Build:

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build . --clean-first
```

Usage:

```bash
./chassis_velocity <can_ifname> <forward> <strafe> <rotate> <duration_ms> [max_translate_rpm] [max_rotate_rpm]
```

The three command values are normalized:

```text
forward: -1.0 to 1.0
strafe:  -1.0 to 1.0
rotate:  -1.0 to 1.0
```

Recommended first tests:

```bash
./chassis_velocity can0 0.4 0.0 0.0 3000 500 400
./chassis_velocity can0 -0.4 0.0 0.0 3000 500 400
./chassis_velocity can0 0.0 0.4 0.0 3000 500 400
./chassis_velocity can0 0.0 -0.4 0.0 3000 500 400
./chassis_velocity can0 0.0 0.0 0.3 3000 500 400
./chassis_velocity can0 0.0 0.0 -0.3 3000 500 400
```

Combined motion test:

```bash
./chassis_velocity can0 0.3 0.2 0.0 3000 500 400
```

Safety behavior:

- The program waits until all four motors have feedback.
- It sends commands every 10 ms.
- It sends zero current when the duration ends.
- It sends zero current when interrupted by `Ctrl+C`.
- If any motor feedback times out, the current command is forced to zero.

Use this tool after `test_chassis_command` has confirmed that front/back,
left/right, and rotation directions are all correct.
