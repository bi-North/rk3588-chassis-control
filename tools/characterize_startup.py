#!/usr/bin/env python3

import argparse
import csv
import socket
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, Tuple


DIRECTIONS: Dict[str, Tuple[float, float, float]] = {
    "forward": (1.0, 0.0, 0.0),
    "back": (-1.0, 0.0, 0.0),
    "right": (0.0, 1.0, 0.0),
    "left": (0.0, -1.0, 0.0),
    "rotate-left": (0.0, 0.0, 1.0),
    "rotate-right": (0.0, 0.0, -1.0),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure the loaded chassis startup threshold with operator confirmation."
    )
    parser.add_argument("direction", choices=sorted(DIRECTIONS))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=20001)
    parser.add_argument("--duration-ms", type=int, default=1500)
    parser.add_argument("--period-ms", type=int, default=100)
    parser.add_argument(
        "--scales",
        type=float,
        nargs="+",
        default=[0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8],
    )
    parser.add_argument("--output")
    return parser.parse_args()


def send_command(
    sock: socket.socket,
    address: Tuple[str, int],
    values: Tuple[float, float, float],
) -> None:
    payload = f"{values[0]:.4f} {values[1]:.4f} {values[2]:.4f}\n".encode("ascii")
    sock.sendto(payload, address)


def send_stop(sock: socket.socket, address: Tuple[str, int]) -> None:
    for _ in range(3):
        send_command(sock, address, (0.0, 0.0, 0.0))
        time.sleep(0.02)


def run_trial(
    sock: socket.socket,
    address: Tuple[str, int],
    direction: Tuple[float, float, float],
    scale: float,
    duration_ms: int,
    period_ms: int,
) -> None:
    command = tuple(component * scale for component in direction)
    deadline = time.monotonic() + (duration_ms / 1000.0)
    period_s = period_ms / 1000.0
    try:
        while time.monotonic() < deadline:
            send_command(sock, address, command)
            time.sleep(period_s)
    finally:
        send_stop(sock, address)


def ask_result() -> str:
    while True:
        answer = input("Did the chassis start moving reliably? [y/n/q]: ").strip().lower()
        if answer in {"y", "n", "q"}:
            return answer


def main() -> int:
    args = parse_args()
    if args.duration_ms <= 0 or args.period_ms <= 0:
        raise SystemExit("duration and period must be positive")
    if not args.scales:
        raise SystemExit("provide at least one scale")

    address = (args.host, args.port)
    direction = DIRECTIONS[args.direction]
    output_path = Path(args.output or f"../docs/startup_characterization_{args.direction}.csv")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    rows = []

    print("Keep the chassis clear. Start chassis_daemon in another terminal first.")
    print(f"Direction={args.direction} endpoint=udp://{args.host}:{args.port}")

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for raw_scale in args.scales:
            scale = max(0.0, min(1.0, raw_scale))
            input(f"\nPress Enter to run scale={scale:.2f}, or Ctrl+C to abort safely.")
            print(f"Running scale={scale:.2f} for {args.duration_ms}ms...")
            run_trial(sock, address, direction, scale, args.duration_ms, args.period_ms)
            answer = ask_result()
            if answer == "q":
                print("Stopped by operator.")
                break
            rows.append(
                {
                    "timestamp": datetime.now().isoformat(timespec="seconds"),
                    "direction": args.direction,
                    "scale": f"{scale:.2f}",
                    "duration_ms": args.duration_ms,
                    "moved_reliably": "yes" if answer == "y" else "no",
                }
            )

    with output_path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(
            output_file,
            fieldnames=["timestamp", "direction", "scale", "duration_ms", "moved_reliably"],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote report: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
