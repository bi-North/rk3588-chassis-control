#!/usr/bin/env python3
"""
Grid-search helper for single-motor 3508 speed PID parameters.

This script repeatedly runs test_speed_pid with different Kp/Ki values, parses
the printed rpm/error lines, and stops when the selected settling window meets
the acceptance thresholds.
"""

from __future__ import annotations

import argparse
import math
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SAMPLE_RE = re.compile(
    r"target=\s*(?P<target>-?\d+)\s+"
    r"rpm=\s*(?P<rpm>-?\d+)\s+"
    r"cmd_current=\s*(?P<cmd>-?\d+)\s+"
    r"fb_current=\s*(?P<fb>-?\d+)\s+"
    r"temp=\s*(?P<temp>-?\d+)C\s+"
    r"err=\s*(?P<err>-?\d+(?:\.\d+)?)"
)


@dataclass
class Sample:
    rpm: int
    cmd_current: int
    fb_current: int
    err: float


@dataclass
class TrialResult:
    kp: float
    ki: float
    kd: float
    ok: bool
    score: float
    mean_abs_err: float
    max_abs_err: float
    rpm_std: float
    max_abs_cmd: int
    sample_count: int
    return_code: int


def frange(start: float, stop: float, step: float) -> list[float]:
    if step <= 0:
        raise ValueError("step must be positive")

    values: list[float] = []
    value = start
    epsilon = step / 1000.0
    while value <= stop + epsilon:
        values.append(round(value, 6))
        value += step
    return values


def parse_samples(output: str) -> list[Sample]:
    samples: list[Sample] = []
    for line in output.splitlines():
        match = SAMPLE_RE.search(line)
        if not match:
            continue

        samples.append(
            Sample(
                rpm=int(match.group("rpm")),
                cmd_current=int(match.group("cmd")),
                fb_current=int(match.group("fb")),
                err=float(match.group("err")),
            )
        )
    return samples


def evaluate_trial(
    kp: float,
    ki: float,
    kd: float,
    samples: list[Sample],
    return_code: int,
    settle_fraction: float,
    mean_err_limit: float,
    max_err_limit: float,
    rpm_std_limit: float,
    max_cmd_limit: int,
    min_samples: int,
) -> TrialResult:
    if not samples:
        return TrialResult(kp, ki, kd, False, math.inf, math.inf, math.inf, math.inf, 0, 0, return_code)

    start_index = int(len(samples) * settle_fraction)
    window = samples[start_index:]
    if len(window) < min_samples:
        window = samples[-min_samples:] if len(samples) >= min_samples else samples

    abs_errors = [abs(sample.err) for sample in window]
    rpms = [sample.rpm for sample in window]
    commands = [abs(sample.cmd_current) for sample in window]

    mean_abs_err = statistics.fmean(abs_errors)
    max_abs_err = max(abs_errors)
    rpm_std = statistics.pstdev(rpms) if len(rpms) > 1 else 0.0
    max_abs_cmd = max(commands) if commands else 0

    ok = (
        return_code == 0
        and len(window) >= min_samples
        and mean_abs_err <= mean_err_limit
        and max_abs_err <= max_err_limit
        and rpm_std <= rpm_std_limit
        and max_abs_cmd <= max_cmd_limit
    )

    score = mean_abs_err + (0.35 * max_abs_err) + (0.25 * rpm_std) + (0.01 * max_abs_cmd)

    return TrialResult(
        kp=kp,
        ki=ki,
        kd=kd,
        ok=ok,
        score=score,
        mean_abs_err=mean_abs_err,
        max_abs_err=max_abs_err,
        rpm_std=rpm_std,
        max_abs_cmd=max_abs_cmd,
        sample_count=len(window),
        return_code=return_code,
    )


def run_trial(args: argparse.Namespace, kp: float, ki: float) -> tuple[TrialResult, str]:
    command = [
        str(args.exe),
        args.ifname,
        str(args.motor),
        str(args.target),
        str(args.duration_ms),
        f"{kp:.6g}",
        f"{ki:.6g}",
        f"{args.kd:.6g}",
        str(args.period_ms),
    ]

    timeout_s = (args.duration_ms / 1000.0) + args.timeout_margin_s
    completed = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout_s,
        check=False,
    )

    samples = parse_samples(completed.stdout)
    result = evaluate_trial(
        kp=kp,
        ki=ki,
        kd=args.kd,
        samples=samples,
        return_code=completed.returncode,
        settle_fraction=args.settle_fraction,
        mean_err_limit=args.mean_err_limit,
        max_err_limit=args.max_err_limit,
        rpm_std_limit=args.rpm_std_limit,
        max_cmd_limit=args.max_cmd_limit,
        min_samples=args.min_samples,
    )
    return result, completed.stdout


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Auto tune one 3508 motor speed PID by running test_speed_pid repeatedly.")
    parser.add_argument("--exe", default="./test_speed_pid", type=Path, help="Path to test_speed_pid executable.")
    parser.add_argument("--ifname", default="can0", help="SocketCAN interface name.")
    parser.add_argument("--motor", type=int, required=True, choices=[1, 2, 3, 4], help="Motor ID 1..4.")
    parser.add_argument("--target", type=int, required=True, help="Target rpm, for example 300 or -300.")
    parser.add_argument("--duration-ms", type=int, default=5000, help="Duration of each trial.")
    parser.add_argument("--period-ms", type=int, default=10, help="Control period passed to test_speed_pid.")
    parser.add_argument("--kp", nargs=3, type=float, metavar=("START", "STOP", "STEP"), required=True)
    parser.add_argument("--ki", nargs=3, type=float, metavar=("START", "STOP", "STEP"), required=True)
    parser.add_argument("--kd", type=float, default=0.0)
    parser.add_argument("--settle-fraction", type=float, default=0.5, help="Ignore the first fraction of samples when evaluating.")
    parser.add_argument("--mean-err-limit", type=float, default=35.0, help="Pass if mean abs error in settling window is <= this rpm.")
    parser.add_argument("--max-err-limit", type=float, default=90.0, help="Pass if max abs error in settling window is <= this rpm.")
    parser.add_argument("--rpm-std-limit", type=float, default=45.0, help="Pass if rpm standard deviation is <= this rpm.")
    parser.add_argument("--max-cmd-limit", type=int, default=600, help="Pass if max abs command current is <= this value.")
    parser.add_argument("--min-samples", type=int, default=8, help="Minimum samples required in the settling window.")
    parser.add_argument("--timeout-margin-s", type=float, default=3.0)
    parser.add_argument("--show-output", action="store_true", help="Print full test_speed_pid output for every trial.")
    parser.add_argument("--no-stop-on-pass", action="store_true", help="Continue all trials even after a passing result.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if not args.exe.exists():
        print(f"error: executable not found: {args.exe}", file=sys.stderr)
        return 2

    kp_values = frange(args.kp[0], args.kp[1], args.kp[2])
    ki_values = frange(args.ki[0], args.ki[1], args.ki[2])

    print("Auto tune speed PID")
    print(f"  motor=M{args.motor} target={args.target}rpm ifname={args.ifname}")
    print(f"  kp={kp_values}")
    print(f"  ki={ki_values}")
    print("  Lift the chassis. Each trial will stop by sending zero current through test_speed_pid.")

    best: TrialResult | None = None
    trial_index = 0
    total_trials = len(kp_values) * len(ki_values)

    for kp in kp_values:
        for ki in ki_values:
            trial_index += 1
            print(f"\n[{trial_index}/{total_trials}] trial kp={kp:.3f} ki={ki:.3f} kd={args.kd:.3f}")

            try:
                result, output = run_trial(args, kp, ki)
            except subprocess.TimeoutExpired:
                print("  timeout: test process did not exit in time")
                continue

            if args.show_output:
                print(output)

            print(
                "  "
                f"ok={result.ok} "
                f"score={result.score:.1f} "
                f"mean_abs_err={result.mean_abs_err:.1f} "
                f"max_abs_err={result.max_abs_err:.1f} "
                f"rpm_std={result.rpm_std:.1f} "
                f"max_abs_cmd={result.max_abs_cmd} "
                f"samples={result.sample_count} "
                f"rc={result.return_code}"
            )

            if best is None or result.score < best.score:
                best = result

            if result.ok and not args.no_stop_on_pass:
                print("\nPASS")
                print(f"Recommended: kp={result.kp:.3f} ki={result.ki:.3f} kd={result.kd:.3f}")
                return 0

    print("\nDONE")
    if best is not None:
        print(
            "Best tried: "
            f"kp={best.kp:.3f} ki={best.ki:.3f} kd={best.kd:.3f} "
            f"ok={best.ok} score={best.score:.1f} "
            f"mean_abs_err={best.mean_abs_err:.1f} "
            f"max_abs_err={best.max_abs_err:.1f} "
            f"rpm_std={best.rpm_std:.1f} "
            f"max_abs_cmd={best.max_abs_cmd}"
        )
        return 0 if best.ok else 1

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
