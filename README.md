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
sudo ip link set can0 type can bitrate 1000000 restart-ms 100 berr-reporting on
sudo ip link set can0 up
ip -details link show can0
```

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

## Record Results

After testing, fill in `docs/motor_mapping.md`:

- Which physical wheel maps to motor IDs 1 to 4
- Which direction each wheel rotates for positive current
- Whether any motor direction needs inversion in software
