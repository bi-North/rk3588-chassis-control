#!/usr/bin/env bash

set -euo pipefail

ifname="${1:-can0}"
bitrate="${2:-1000000}"

modprobe can || true
modprobe can_raw || true
modprobe gs_usb || true

ip link set "${ifname}" down 2>/dev/null || true
ip link set "${ifname}" type can bitrate "${bitrate}"
ip link set "${ifname}" up
ip -details link show "${ifname}"
