#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
systemd_dir="/etc/systemd/system"
run_user="${SUDO_USER:-root}"

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run this installer with sudo." >&2
    exit 1
fi

if [[ ! -x "${project_root}/build/chassis_daemon" ]]; then
    echo "Build chassis_daemon before installing the services." >&2
    echo "Run: cd ${project_root}/build && cmake --build . --clean-first" >&2
    exit 1
fi

sed "s|@PROJECT_ROOT@|${project_root}|g" \
    "${project_root}/systemd/rk3588-can.service.in" \
    > "${systemd_dir}/rk3588-can.service"

sed -e "s|@PROJECT_ROOT@|${project_root}|g" \
    -e "s|@RUN_USER@|${run_user}|g" \
    "${project_root}/systemd/rk3588-chassis.service.in" \
    > "${systemd_dir}/rk3588-chassis.service"

systemctl daemon-reload

echo "Installed:"
echo "  ${systemd_dir}/rk3588-can.service"
echo "  ${systemd_dir}/rk3588-chassis.service"
echo
echo "Review the service files, then enable them explicitly:"
echo "  sudo systemctl enable --now rk3588-can.service"
echo "  sudo systemctl enable --now rk3588-chassis.service"
