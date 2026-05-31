#!/usr/bin/env python3

import argparse
import socket
import time
from typing import Tuple


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send normalized velocity commands to chassis_daemon."
    )
    parser.add_argument("forward", type=float)
    parser.add_argument("strafe", type=float)
    parser.add_argument("rotate", type=float)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=20001)
    parser.add_argument("--duration-ms", type=int, default=3000)
    parser.add_argument("--period-ms", type=int, default=100)
    return parser.parse_args()


def send_command(sock: socket.socket, address: Tuple[str, int], values: Tuple[float, float, float]) -> None:
    payload = f"{values[0]:.4f} {values[1]:.4f} {values[2]:.4f}\n".encode("ascii")
    sock.sendto(payload, address)


def main() -> int:
    args = parse_args()
    if args.duration_ms <= 0:
        raise SystemExit("--duration-ms must be positive")
    if args.period_ms <= 0:
        raise SystemExit("--period-ms must be positive")

    command = (
        max(-1.0, min(1.0, args.forward)),
        max(-1.0, min(1.0, args.strafe)),
        max(-1.0, min(1.0, args.rotate)),
    )
    address = (args.host, args.port)
    deadline = time.monotonic() + (args.duration_ms / 1000.0)
    period_s = args.period_ms / 1000.0

    print(
        f"Sending cmd={command} to udp://{args.host}:{args.port} "
        f"for {args.duration_ms}ms every {args.period_ms}ms"
    )

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        try:
            while time.monotonic() < deadline:
                send_command(sock, address, command)
                time.sleep(period_s)
        except KeyboardInterrupt:
            print("Interrupted.")
        finally:
            for _ in range(3):
                send_command(sock, address, (0.0, 0.0, 0.0))
                time.sleep(0.02)

    print("Sent stop command.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
