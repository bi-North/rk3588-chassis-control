# Chassis Keyboard Test

This tool tests four-motor chassis control from Linux through SocketCAN.

## Preconditions

- The chassis is lifted for the first tests.
- `monitor_3508` can see all four motors.
- Single-motor PID tests have passed.
- Motor mapping is recorded in `docs/motor_mapping.md`.

## Build

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build .
```

Expected:

```bash
ls test_chassis_keyboard
```

## Run

Start with low speed limits:

```bash
./test_chassis_keyboard can0 500 400
```

Keys:

```text
w/s   forward/backward
a/d   left/right strafe
q/e   rotate left/right
+/-   adjust speed scale
space stop now
x     exit
```

The tool uses key-repeat behavior. Hold a key to keep motion. If no command key
arrives for about 300 ms, the command automatically returns to zero.

## Lifted Test Order

1. Hold `w` briefly. Check that the four wheels match forward motion.
2. Hold `s` briefly. Check reverse.
3. Hold `a` briefly. Check left strafe.
4. Hold `d` briefly. Check right strafe.
5. Hold `q` and `e` briefly. Check rotation.
6. Press `space` and confirm all currents go to zero.
7. Press `x` and confirm the tool sends zero current before exiting.

If a direction is wrong, do not tune PID first. Fix motor mapping or sign in the
chassis control layer.
