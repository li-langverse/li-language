#!/usr/bin/env python3
"""Correctness gate for benchmark sources (Tier 0 / Tier 2 invariants)."""

from __future__ import annotations

import argparse
import csv
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LIC = REPO / "build" / "compiler" / "lic" / "lic"
RESULTS = REPO / "benchmarks" / "results"


def lic_build(path: Path) -> bool:
    if not LIC.is_file():
        print(f"lic missing at {LIC}", file=sys.stderr)
        return False
    env = {**os.environ, "LIC": str(LIC)}
    proc = subprocess.run(
        [str(LIC), "build", str(path), "-o", "/dev/null"],
        cwd=REPO,
        env=env,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(proc.stderr or proc.stdout, file=sys.stderr)
    return proc.returncode == 0


def tier0_sources() -> list[Path]:
    root = REPO / "li-tests" / "benchmarks" / "tier0_correctness"
    return sorted(root.glob("*.li"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write-csv", type=Path, default=RESULTS / "verify.csv")
    args = parser.parse_args()

    rows: list[list[object]] = []
    ok = True
    for src in tier0_sources():
        passed = lic_build(src)
        ok = ok and passed
        rows.append(
            [
                src.stem,
                "li",
                "verify",
                1,
                "invariant",
                1 if passed else 0,
                "bool",
                "",
                "",
                "",
                passed,
            ]
        )
        status = "PASS" if passed else "FAIL"
        print(f"{status} verify {src.relative_to(REPO)}")

    args.write_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.write_csv.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "benchmark",
                "lang",
                "variant",
                "threads",
                "metric",
                "value",
                "unit",
                "git_sha",
                "cpu_model",
                "flags",
                "passed",
            ]
        )
        w.writerows(rows)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
