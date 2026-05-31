#!/usr/bin/env python3

import argparse
import socket
import struct
import time


MAGIC = 0x524B4348
VERSION = 1
MSG_HEARTBEAT = 3
HEADER_FORMAT = "!IHHIIHH"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send protocol heartbeat packets to the RT-Thread UDP echo endpoint."
    )
    parser.add_argument("--peer", default="10.10.10.30")
    parser.add_argument("--bind", default="10.10.10.31")
    parser.add_argument("--port", type=int, default=21001)
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--interval-ms", type=int, default=100)
    parser.add_argument("--timeout-ms", type=int, default=500)
    return parser.parse_args()


def build_heartbeat(sequence: int) -> bytes:
    monotonic_ms = int(time.monotonic() * 1000) & 0xFFFFFFFF
    return struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        MSG_HEARTBEAT,
        sequence,
        monotonic_ms,
        0,
        0,
    )


def main() -> int:
    args = parse_args()
    if args.count <= 0:
        raise SystemExit("--count must be positive")
    if args.interval_ms <= 0:
        raise SystemExit("--interval-ms must be positive")
    if args.timeout_ms <= 0:
        raise SystemExit("--timeout-ms must be positive")

    peer = (args.peer, args.port)
    received = 0
    total_latency_ms = 0.0

    print(
        f"RT-Thread UDP echo probe: local={args.bind} peer={args.peer}:{args.port} "
        f"count={args.count}"
    )

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.bind, 0))
        sock.settimeout(args.timeout_ms / 1000.0)

        for sequence in range(1, args.count + 1):
            packet = build_heartbeat(sequence)
            started = time.monotonic()
            sock.sendto(packet, peer)
            try:
                response, address = sock.recvfrom(2048)
            except socket.timeout:
                print(f"seq={sequence} timeout")
            else:
                latency_ms = (time.monotonic() - started) * 1000.0
                if address != peer:
                    print(f"seq={sequence} ignored response from {address[0]}:{address[1]}")
                elif response != packet:
                    print(f"seq={sequence} invalid echo length={len(response)}")
                else:
                    received += 1
                    total_latency_ms += latency_ms
                    print(f"seq={sequence} ok latency={latency_ms:.3f}ms")
            time.sleep(args.interval_ms / 1000.0)

    lost = args.count - received
    print(f"summary: sent={args.count} received={received} lost={lost}")
    if received:
        print(f"average latency={total_latency_ms / received:.3f}ms")
    return 0 if lost == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
