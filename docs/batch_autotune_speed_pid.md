# Batch Auto Tune Speed PID

Use `tools/batch_autotune_speed_pid.py` after one motor has already been tuned
manually and `autotune_speed_pid.py` works.

## Build First

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build .
```

## Tune Motors 1 to 3

Lift the chassis, then run from the `build` directory:

```bash
python3 ../tools/batch_autotune_speed_pid.py \
  --exe "$(pwd)/test_speed_pid" \
  --ifname can0 \
  --motors 1,2,3 \
  --targets 300,-300 \
  --duration-ms 8000 \
  --kp 2.0 3.2 0.2 \
  --ki 0.3 0.9 0.1 \
  --mean-err-limit 45 \
  --max-err-limit 110 \
  --rpm-std-limit 50
```

The script writes:

```text
docs/pid_autotune_results.md
```

Rows marked `PASS` meet the thresholds. Rows marked `BEST` are the best
candidate found but did not satisfy every threshold.

## Shorter Test Range

If the full scan takes too long, start with:

```bash
python3 ../tools/batch_autotune_speed_pid.py \
  --exe "$(pwd)/test_speed_pid" \
  --ifname can0 \
  --motors 1,2,3 \
  --targets 300,-300 \
  --duration-ms 5000 \
  --kp 2.0 2.8 0.2 \
  --ki 0.4 0.8 0.1
```
