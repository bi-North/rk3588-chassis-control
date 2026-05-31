# Linux Service Installation

Use these services after the Linux chassis daemon has passed manual validation.

Two services are provided:

```text
rk3588-can.service      configures can0 at 1000000 bit/s
rk3588-chassis.service  starts chassis_daemon after can0 is ready
```

The installer copies the service files but does not enable or start them
automatically. CAN setup runs as root. The chassis daemon runs as the ordinary
user that invoked `sudo`.

## Build

```bash
cd ~/projects/rk3588-chassis-control && git pull && cd build && cmake --build . --clean-first
```

## Test CAN Setup Script

Run this once manually:

```bash
cd ~/projects/rk3588-chassis-control && sudo ./scripts/setup_can.sh can0 1000000
```

Expected output includes:

```text
can state ERROR-ACTIVE
bitrate 1000000
```

The script intentionally omits optional `restart-ms` and `berr-reporting`
settings because some USB-CAN adapters or kernel drivers reject them.

## Install Services

```bash
cd ~/projects/rk3588-chassis-control && sudo ./scripts/install_systemd.sh
```

Review the generated files:

```bash
systemctl cat rk3588-can.service
systemctl cat rk3588-chassis.service
```

## Enable Services

Enable CAN setup first:

```bash
sudo systemctl enable --now rk3588-can.service
```

Confirm:

```bash
systemctl status rk3588-can.service --no-pager
ip -details link show can0
```

Then enable the chassis daemon:

```bash
sudo systemctl enable --now rk3588-chassis.service
```

Confirm:

```bash
systemctl status rk3588-chassis.service --no-pager
journalctl -u rk3588-chassis.service -n 30 --no-pager
```

## Stop Or Disable

Stop the daemon:

```bash
sudo systemctl stop rk3588-chassis.service
```

Disable automatic startup:

```bash
sudo systemctl disable rk3588-chassis.service
sudo systemctl disable rk3588-can.service
```

## Notes

- The daemon still binds UDP to `127.0.0.1:20001`.
- The daemon starts with zero velocity and sends zero current until it receives
  a command.
- The daemon still stops the chassis if no new command arrives for 300 ms.
- Keep manual control available while installing and validating the service.
