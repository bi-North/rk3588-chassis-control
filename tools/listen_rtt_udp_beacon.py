#!/usr/bin/env python3

import argparse
import socket
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Listen for RT-Thread UDP beacon packets on the Linux side."
    )
    parser.add_argument("--bind", default="10.10.10.31")
    parser.add_argument("--port", type=int, default=21002)
    parser.add_argument("--timeout-s", type=float, default=20.0)
    parser.add_argument("--count", type=int, default=3)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.timeout_s <= 0:
        raise SystemExit("--timeout-s must be positive")
    if args.count <= 0:
        raise SystemExit("--count must be positive")

    received = 0
    deadline = time.monotonic() + args.timeout_s

    print(
        f"RT-Thread UDP beacon listener: local={args.bind}:{args.port} "
        f"timeout={args.timeout_s:.1f}s count={args.count}"
    )

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.bind, args.port))
        sock.settimeout(0.5)

        while received < args.count and time.monotonic() < deadline:
            try:
                packet, address = sock.recvfrom(2048)
            except socket.timeout:
                continue

            received += 1
            text = packet.decode("utf-8", errors="replace")
            print(f"beacon={received} from={address[0]}:{address[1]} data={text!r}")

    print(f"summary: received={received} expected={args.count}")
    return 0 if received >= args.count else 1


if __name__ == "__main__":
    raise SystemExit(main())
