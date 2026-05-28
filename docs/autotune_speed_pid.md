# Auto Tune Speed PID

`tools/autotune_speed_pid.py` runs `test_speed_pid` repeatedly with different
`Kp/Ki` values and evaluates the settling part of each run.

## Safety

- Lift the chassis before running this tool.
- Use low target speed first, such as `300` or `-300` rpm.
- Keep `Kd=0` until all motor directions and mappings are confirmed.
- Stop the script with `Ctrl+C` if a motor behaves unexpectedly.

Each trial is still executed by `test_speed_pid`, so zero current is sent when
the trial exits.

## Build First

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build .
```

## Example: Tune M4 Negative Direction

From the `build` directory:

```bash
python3 ../tools/autotune_speed_pid.py \
  --exe ./test_speed_pid \
  --ifname can0 \
  --motor 4 \
  --target -300 \
  --duration-ms 5000 \
  --kp 2.0 3.0 0.2 \
  --ki 0.2 0.6 0.1
```

The script prints one summary per trial:

```text
ok=False score=... mean_abs_err=... max_abs_err=... rpm_std=... max_abs_cmd=...
```

It stops early when a trial meets the default thresholds and prints:

```text
PASS
Recommended: kp=... ki=... kd=...
```

## Stricter Or Looser Thresholds

Default pass thresholds:

```text
mean absolute error <= 35 rpm
max absolute error  <= 90 rpm
rpm standard dev    <= 45 rpm
max command current <= 600
```

For a rough first pass, loosen the thresholds:

```bash
python3 ../tools/autotune_speed_pid.py \
  --exe ./test_speed_pid \
  --ifname can0 \
  --motor 4 \
  --target -300 \
  --duration-ms 5000 \
  --kp 2.0 3.0 0.2 \
  --ki 0.2 0.6 0.1 \
  --mean-err-limit 50 \
  --max-err-limit 120 \
  --rpm-std-limit 60
```

## Continue All Trials

If you want to compare every combination instead of stopping at the first pass:

```bash
python3 ../tools/autotune_speed_pid.py \
  --exe ./test_speed_pid \
  --ifname can0 \
  --motor 4 \
  --target -300 \
  --duration-ms 5000 \
  --kp 2.0 3.0 0.2 \
  --ki 0.2 0.6 0.1 \
  --no-stop-on-pass
```
