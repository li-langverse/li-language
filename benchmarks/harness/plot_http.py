#!/usr/bin/env python3
"""Plot tier5_http results (stub — full charts land with benchmarks Pages refresh)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RESULTS = REPO / "benchmarks" / "results"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", type=Path, default=RESULTS / "latest.csv")
    args = parser.parse_args()
    if not args.csv.is_file():
        print(f"plot_http: missing {args.csv}", file=sys.stderr)
        return 1
    print(f"plot_http: stub ok ({args.csv.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
