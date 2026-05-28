# Speed PID Test Plan

This test validates one motor speed loop at a time.

## Preconditions

- `monitor_3508` can read stable feedback from `0x201` to `0x204`.
- `test_single_motor` can drive each motor with periodic current commands.
- The chassis is lifted and wheels are free to rotate.
- `docs/motor_mapping.md` has the current motor-to-wheel mapping.

## Build

```bash
cd ~/projects/rk3588-chassis-control
cd build
cmake --build .
```

Expected executables:

```bash
ls monitor_3508 test_single_motor test_speed_pid
```

## Basic Test

Run low target speed first:

```bash
./test_speed_pid can0 1 300 3000
./test_speed_pid can0 2 300 3000
./test_speed_pid can0 3 300 3000
./test_speed_pid can0 4 300 3000
```

Then test reverse direction:

```bash
./test_speed_pid can0 1 -300 3000
./test_speed_pid can0 2 -300 3000
./test_speed_pid can0 3 -300 3000
./test_speed_pid can0 4 -300 3000
```

## Acceptance Criteria

- The tested motor rotates continuously during the test.
- `rpm` moves toward `target`.
- `cmd_current` returns to zero after the tool exits.
- No `BUS-OFF` appears in `ip -details link show can0`.
- Feedback does not timeout.

## If It Oscillates

Try lower proportional gain:

```bash
./test_speed_pid can0 1 300 3000 1.0 0.0 0.0
```

If response is too weak, increase gradually:

```bash
./test_speed_pid can0 1 300 3000 2.2 0.0 0.0
```

Keep `ki=0` and `kd=0` until basic direction and mapping are confirmed.
