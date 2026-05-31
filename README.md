# RK3588 Chassis Control

Linux SocketCAN chassis control test project for DJI 3508 motors.

## Current Stage

Stage 1 validates the RK3588 Linux to USB-CAN to 3508 motor chain:

- Open `can0`
- Receive motor feedback frames `0x201` to `0x204`
- Send motor current command frame `0x200`
- Test one motor at a time with periodic current commands
- Always send zero current before exiting

## Prepare CAN

```bash
sudo modprobe can
sudo modprobe can_raw
sudo modprobe gs_usb

sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000
sudo ip link set can0 up
ip -details link show can0
```

Optional recovery and diagnostic settings can be tried separately. Some
USB-CAN adapters or kernel drivers do not support both options:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can restart-ms 100
sudo ip link set can0 type can berr-reporting on
sudo ip link set can0 up
```

If either optional command fails, keep the working bitrate configuration and
continue testing. `restart-ms` enables automatic recovery after a bus-off
condition. `berr-reporting` enables CAN error frame reporting for diagnostics.

Expected state:

```text
can state ERROR-ACTIVE
bitrate 1000000
```

## Build

```bash
sudo apt update
sudo apt install -y git cmake build-essential can-utils

mkdir -p build
cd build
cmake ..
cmake --build .
```

## Monitor Feedback

```bash
./monitor_3508 can0
```

You should see motor feedback from `0x201` to `0x204`.

## Single Motor Test

Lift the chassis before sending non-zero current.

```bash
# iface motor_id current duration_ms [period_ms]
./test_single_motor can0 1 500 2000
./test_single_motor can0 2 500 2000
./test_single_motor can0 3 500 2000
./test_single_motor can0 4 500 2000
```

The tool sends current commands every 10 ms by default and sends zero current on exit.

## Single Motor Speed PID Test

Lift the chassis before running closed-loop speed control.

```bash
# iface motor_id target_rpm duration_ms [kp ki kd] [period_ms]
./test_speed_pid can0 1 500 5000
./test_speed_pid can0 1 -500 5000
```

Default parameters:

```text
period_ms = 10
kp = 1.80
ki = 0.00
kd = 0.00
current limit = +/-6500
target limit = +/-3800 rpm
```

If the motor direction is opposite to the expected direction, record it in
`docs/motor_mapping.md`. Do not fix signs blindly before all four motors are
mapped.

## Auto Tune Speed PID

After `test_speed_pid` works, you can sweep `Kp/Ki` automatically:

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

See `docs/autotune_speed_pid.md` for thresholds and safety notes.

To tune several motors and directions in one run:

```bash
python3 ../tools/batch_autotune_speed_pid.py \
  --exe "$(pwd)/test_speed_pid" \
  --ifname can0 \
  --motors 1,2,3 \
  --targets 300,-300 \
  --duration-ms 8000 \
  --kp 2.0 3.2 0.2 \
  --ki 0.3 0.9 0.1
```

See `docs/batch_autotune_speed_pid.md`.

## Chassis Keyboard Test

After all four motors have usable PID parameters:

```bash
./test_chassis_keyboard can0 500 400
```

Keys:

```text
w/s forward/backward
a/d left/right strafe
q/e rotate left/right
space stop
x exit
```

See `docs/chassis_keyboard_test.md`.

If the remote desktop or input method does not pass keys into the terminal, use
the command based chassis test instead:

```bash
./test_chassis_command can0 forward 0.3 2000 500 400
./test_chassis_command can0 back 0.3 2000 500 400
./test_chassis_command can0 left 0.3 2000 500 400
./test_chassis_command can0 right 0.3 2000 500 400
./test_chassis_command can0 rotate-left 0.3 2000 500 400
./test_chassis_command can0 rotate-right 0.3 2000 500 400
```

Each command runs for the requested duration and then sends zero current.

For the next integration step, use the normalized velocity interface. It accepts
`forward`, `strafe`, and `rotate` directly:

```bash
./chassis_velocity can0 0.4 0.0 0.0 3000 500 400
./chassis_velocity can0 0.0 0.4 0.0 3000 500 400
./chassis_velocity can0 0.0 0.0 0.3 3000 500 400
./chassis_velocity can0 0.3 0.2 0.0 3000 500 400
```

This is the preferred interface for later ROS2 or RT-Thread bridge work.

## Long-running Chassis Daemon

After validating `chassis_velocity`, run the persistent UDP service:

```bash
./chassis_daemon can0
```

In another terminal, send normalized velocity commands:

```bash
python3 ../tools/send_velocity.py 0.8 0.0 0.0 --duration-ms 3000
python3 ../tools/send_velocity.py 0.0 0.8 0.0 --duration-ms 3000
python3 ../tools/send_velocity.py 0.0 0.0 0.3 --duration-ms 3000
```

The default UDP endpoint is `127.0.0.1:20001`. The daemon keeps a 100 Hz
control loop and stops the chassis if it receives no new command for 300 ms.

See `docs/chassis_daemon.md`.

## Record Results

After testing, fill in `docs/motor_mapping.md`:

- Which physical wheel maps to motor IDs 1 to 4
- Which direction each wheel rotates for positive current
- Whether any motor direction needs inversion in software
