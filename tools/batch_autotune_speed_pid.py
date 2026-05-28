#!/usr/bin/env python3
"""
Batch tune 3508 speed PID parameters for multiple motors and directions.

The script wraps autotune_speed_pid.py and writes a Markdown summary table.
It is intended for bench testing with the chassis lifted.
"""

from __future__ import annotations

import argparse
import datetime as dt
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


PASS_RE = re.compile(r"Recommended:\s+kp=(?P<kp>-?\d+(?:\.\d+)?)\s+ki=(?P<ki>-?\d+(?:\.\d+)?)\s+kd=(?P<kd>-?\d+(?:\.\d+)?)")
BEST_RE = re.compile(
    r"Best tried:\s+kp=(?P<kp>-?\d+(?:\.\d+)?)\s+ki=(?P<ki>-?\d+(?:\.\d+)?)\s+kd=(?P<kd>-?\d+(?:\.\d+)?)\s+"
    r"ok=(?P<ok>True|False)\s+score=(?P<score>-?\d+(?:\.\d+)?)\s+"
    r"mean_abs_err=(?P<mean>-?\d+(?:\.\d+)?)\s+max_abs_err=(?P<max>-?\d+(?:\.\d+)?)\s+"
    r"rpm_std=(?P<std>-?\d+(?:\.\d+)?)\s+max_abs_cmd=(?P<cmd>\d+)"
)
TRIAL_RE = re.compile(
    r"ok=(?P<ok>True|False)\s+score=(?P<score>-?\d+(?:\.\d+)?)\s+"
    r"mean_abs_err=(?P<mean>-?\d+(?:\.\d+)?)\s+max_abs_err=(?P<max>-?\d+(?:\.\d+)?)\s+"
    r"rpm_std=(?P<std>-?\d+(?:\.\d+)?)\s+max_abs_cmd=(?P<cmd>\d+)"
)


@dataclass
class DirectionResult:
    motor: int
    target: int
    status: str
    kp: str
    ki: str
    kd: str
    mean_abs_err: str
    max_abs_err: str
    rpm_std: str
    max_abs_cmd: str
    return_code: int


def parse_list(text: str) -> list[int]:
    values: list[int] = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        values.append(int(part))
    return values


def resolve_executable(path: Path) -> Path:
    if path.is_absolute():
        return path
    if str(path).startswith("."):
        return path.resolve()
    return path


def parse_result(motor: int, target: int, return_code: int, output: str) -> DirectionResult:
    recommended = PASS_RE.search(output)
    if recommended:
        last_trial = None
        for match in TRIAL_RE.finditer(output):
            last_trial = match
        return DirectionResult(
            motor=motor,
            target=target,
            status="PASS",
            kp=recommended.group("kp"),
            ki=recommended.group("ki"),
            kd=recommended.group("kd"),
            mean_abs_err=last_trial.group("mean") if last_trial else "",
            max_abs_err=last_trial.group("max") if last_trial else "",
            rpm_std=last_trial.group("std") if last_trial else "",
            max_abs_cmd=last_trial.group("cmd") if last_trial else "",
            return_code=return_code,
        )

    best = BEST_RE.search(output)
    if best:
        return DirectionResult(
            motor=motor,
            target=target,
            status="BEST" if best.group("ok") == "False" else "PASS",
            kp=best.group("kp"),
            ki=best.group("ki"),
            kd=best.group("kd"),
            mean_abs_err=best.group("mean"),
            max_abs_err=best.group("max"),
            rpm_std=best.group("std"),
            max_abs_cmd=best.group("cmd"),
            return_code=return_code,
        )

    return DirectionResult(
        motor=motor,
        target=target,
        status="FAILED",
        kp="",
        ki="",
        kd="",
        mean_abs_err="",
        max_abs_err="",
        rpm_std="",
        max_abs_cmd="",
        return_code=return_code,
    )


def markdown_table(results: list[DirectionResult]) -> str:
    lines = [
        "| Motor | Target rpm | Status | Kp | Ki | Kd | Mean abs err | Max abs err | RPM std | Max cmd | Return code |",
        "| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for result in results:
        lines.append(
            f"| M{result.motor} | {result.target} | {result.status} | {result.kp} | {result.ki} | {result.kd} | "
            f"{result.mean_abs_err} | {result.max_abs_err} | {result.rpm_std} | {result.max_abs_cmd} | {result.return_code} |"
        )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Batch tune speed PID for multiple 3508 motors.")
    parser.add_argument("--autotune", default="../tools/autotune_speed_pid.py", type=Path, help="Path to autotune_speed_pid.py.")
    parser.add_argument("--exe", default="./test_speed_pid", type=Path, help="Path to test_speed_pid executable.")
    parser.add_argument("--ifname", default="can0")
    parser.add_argument("--motors", default="1,2,3", help="Comma-separated motor IDs, for example 1,2,3.")
    parser.add_argument("--targets", default="300,-300", help="Comma-separated target rpm values.")
    parser.add_argument("--duration-ms", default=8000, type=int)
    parser.add_argument("--kp", nargs=3, default=[2.0, 3.2, 0.2], type=float, metavar=("START", "STOP", "STEP"))
    parser.add_argument("--ki", nargs=3, default=[0.3, 0.9, 0.1], type=float, metavar=("START", "STOP", "STEP"))
    parser.add_argument("--kd", default=0.0, type=float)
    parser.add_argument("--mean-err-limit", default=45.0, type=float)
    parser.add_argument("--max-err-limit", default=110.0, type=float)
    parser.add_argument("--rpm-std-limit", default=50.0, type=float)
    parser.add_argument("--max-cmd-limit", default=700, type=int)
    parser.add_argument("--output", default="../docs/pid_autotune_results.md", type=Path)
    parser.add_argument("--keep-going", action="store_true", help="Continue if one motor/direction fails.")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    autotune = args.autotune.resolve() if str(args.autotune).startswith(".") else args.autotune
    exe = resolve_executable(args.exe)
    motors = parse_list(args.motors)
    targets = parse_list(args.targets)

    if not autotune.exists():
        print(f"error: autotune script not found: {autotune}", file=sys.stderr)
        return 2
    if not exe.exists():
        print(f"error: test executable not found: {exe}", file=sys.stderr)
        return 2

    print("Batch speed PID autotune")
    print(f"  motors={motors}")
    print(f"  targets={targets}")
    print(f"  exe={exe}")
    print("  Lift the chassis. Press Ctrl+C to abort if anything behaves unexpectedly.")

    results: list[DirectionResult] = []
    total = len(motors) * len(targets)
    index = 0

    try:
        for motor in motors:
            for target in targets:
                index += 1
                print(f"\n=== [{index}/{total}] M{motor} target={target} rpm ===")
                command = [
                    sys.executable,
                    str(autotune),
                    "--exe",
                    str(exe),
                    "--ifname",
                    args.ifname,
                    "--motor",
                    str(motor),
                    "--target",
                    str(target),
                    "--duration-ms",
                    str(args.duration_ms),
                    "--kp",
                    str(args.kp[0]),
                    str(args.kp[1]),
                    str(args.kp[2]),
                    "--ki",
                    str(args.ki[0]),
                    str(args.ki[1]),
                    str(args.ki[2]),
                    "--kd",
                    str(args.kd),
                    "--mean-err-limit",
                    str(args.mean_err_limit),
                    "--max-err-limit",
                    str(args.max_err_limit),
                    "--rpm-std-limit",
                    str(args.rpm_std_limit),
                    "--max-cmd-limit",
                    str(args.max_cmd_limit),
                ]
                completed = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
                print(completed.stdout)
                result = parse_result(motor, target, completed.returncode, completed.stdout)
                results.append(result)

                if result.status == "FAILED" and not args.keep_going:
                    print("Stop on failure. Use --keep-going to continue all tests.")
                    break
            else:
                continue
            break
    except KeyboardInterrupt:
        print("\nInterrupted by user.")

    timestamp = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    report = [
        "# PID Autotune Results",
        "",
        f"Generated: {timestamp}",
        "",
        markdown_table(results),
        "",
        "Use PASS rows directly. BEST rows are the best candidates found but did not meet all thresholds.",
        "",
    ]

    output_path = args.output
    if not output_path.is_absolute():
        output_path = output_path.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(report), encoding="utf-8")
    print(f"\nWrote report: {output_path}")
    print(markdown_table(results))

    return 0 if all(result.status == "PASS" for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
