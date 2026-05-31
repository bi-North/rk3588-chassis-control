#!/usr/bin/env bash

set -u

echo "== Kernel =="
uname -r

echo
echo "== IVSHMEM modules =="
lsmod | grep -E 'ivshmem|Module' || true

echo
echo "== Installed extra modules =="
kernel_release="$(uname -r)"
if [[ -d "/lib/modules/${kernel_release}/extra" ]]; then
    find "/lib/modules/${kernel_release}/extra" -maxdepth 1 -type f -printf '%f\n' | sort
else
    echo "/lib/modules/${kernel_release}/extra does not exist"
fi

echo
echo "== Network interfaces =="
ip -br link

echo
echo "== IP addresses =="
ip -br addr

echo
echo "== Routes =="
ip route

echo
echo "== Expected hybrid virtual NIC =="
if ip link show enp255s5 >/dev/null 2>&1; then
    echo "Found enp255s5"
    ip -details addr show enp255s5
else
    echo "enp255s5 was not found"
fi

echo
echo "== IVSHMEM PCI devices =="
if command -v lspci >/dev/null 2>&1; then
    lspci -nn | grep -i -E 'ivshmem|inter-vm|shared memory|1af4:1110|1af4:1111' || true
else
    echo "lspci is not installed"
fi

echo
echo "== Next manual checks =="
echo "If enp255s5 exists but has no address, configure the documented Linux-side IP:"
echo "  sudo ip addr replace 10.10.10.31/24 dev enp255s5"
echo "  sudo ip link set enp255s5 up"
echo "Then run this probe again and send the complete output."
