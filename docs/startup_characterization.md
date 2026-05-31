# Loaded Startup Characterization

Use this procedure after `chassis_daemon` passes the safety validation. It
measures the minimum normalized command scale that reliably starts the loaded
chassis.

The current observation is:

```text
scale 0.4 did not reliably start the loaded chassis
scale 0.8 started the loaded chassis
```

The goal is to collect a more precise threshold before changing PID parameters
or adding startup compensation.

## Start The Daemon With CSV Logging

Open terminal A:

```bash
cd ~/projects/rk3588-chassis-control/build && ./chassis_daemon can0 127.0.0.1 20001 500 400 ../docs/chassis_daemon_startup.csv
```

## Measure Forward Startup

Open terminal B:

```bash
cd ~/projects/rk3588-chassis-control/build && python3 ../tools/characterize_startup.py forward
```

The script tests scales from `0.2` to `0.8`. Before each trial, press Enter.
After each trial, type:

```text
y  the chassis started moving reliably
n  the chassis did not start reliably
q  stop the measurement
```

The script writes:

```text
docs/startup_characterization_forward.csv
```

The daemon writes:

```text
docs/chassis_daemon_startup.csv
```

## Measure Other Directions

After forward startup is measured, repeat only if needed:

```bash
python3 ../tools/characterize_startup.py back
python3 ../tools/characterize_startup.py right
python3 ../tools/characterize_startup.py left
python3 ../tools/characterize_startup.py rotate-left
python3 ../tools/characterize_startup.py rotate-right
```

## Safety

- Test one direction at a time.
- Keep the test area clear.
- Press `Ctrl+C` in terminal A to stop the daemon.
- The script sends zero velocity after every trial.
- The daemon still applies its 300 ms command timeout.

## Interpretation

If the loaded startup threshold is consistently high while the unloaded motor
PID results remain good, investigate mechanical resistance and add a carefully
bounded startup compensation strategy. Do not increase PID gains blindly.
