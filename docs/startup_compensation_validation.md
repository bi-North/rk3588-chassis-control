# Startup Compensation Validation

The chassis controller applies a short startup boost to overcome loaded static
friction while preserving lower steady-state speed commands.

## Behavior

```text
requested command below 0.20: no boost
requested command 0.20 to 0.74: temporarily scaled to 0.75
requested command 0.75 or above: no additional boost
boost duration: 250 ms
```

The boost triggers only when the command changes from zero to non-zero. It does
not remain active while the chassis is moving.

## Build

```bash
cd ~/projects/rk3588-chassis-control && git pull && cd build && cmake --build . --clean-first
```

## Start The Daemon

Open terminal A:

```bash
cd ~/projects/rk3588-chassis-control/build && ./chassis_daemon can0
```

The daemon prints `boost=1` briefly during compensated startup, then prints
`boost=0`.

## Test Low-Speed Startup

Open terminal B and test one direction at a time:

```bash
cd ~/projects/rk3588-chassis-control/build && python3 ../tools/send_velocity.py 0.4 0.0 0.0 --duration-ms 3000
```

```bash
python3 ../tools/send_velocity.py 0.0 0.4 0.0 --duration-ms 3000
```

```bash
python3 ../tools/send_velocity.py 0.0 0.0 0.4 --duration-ms 3000
```

Expected result:

- The chassis starts moving from rest.
- `boost=1` appears only near startup.
- The daemon returns to `boost=0`.
- The chassis continues moving at the requested lower speed.
- The chassis stops when the sender exits.
- `online=1` and `drops=0` remain stable.

## Safety Check

Send a very small command:

```bash
python3 ../tools/send_velocity.py 0.1 0.0 0.0 --duration-ms 1000
```

Expected result: the daemon must keep `boost=0`. A very small input must not be
amplified into a startup pulse.
