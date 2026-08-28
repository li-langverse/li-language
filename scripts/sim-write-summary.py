#!/usr/bin/env python3
"""Emit li_sim_summary_v1 for Li sim.scientific runs (same helper as verify.py)."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "benchmarks" / "harness"))

from sim_summary import (  # noqa: E402
    SUMMARY_FORMATS,
    build_summary_from_li_run,
    write_summary,
)


def detail_from_env_or_arg(arg: str | None) -> str:
    if arg:
        return arg
    env = os.environ.get("LI_SIM_OUTPUT_DETAIL", "summary")
    return env


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--algo-id", type=int, default=int(os.environ.get("LI_SIM_ALGO_ID", "0")))
    p.add_argument("--ok", type=int, choices=(0, 1), default=int(os.environ.get("LI_SIM_OK", "1")))
    p.add_argument("--checksum", type=float, default=float(os.environ.get("LI_SIM_CHECKSUM", "0")))
    p.add_argument("--vertical-id", type=int, default=int(os.environ.get("LI_SIM_VERTICAL_ID", "0")))
    p.add_argument("--benchmark", default=os.environ.get("LI_SIM_BENCHMARK", ""))
    p.add_argument("--lang", default=os.environ.get("LI_SIM_LANG", "li"))
    p.add_argument("--variant", default=os.environ.get("LI_SIM_VARIANT", "pure_li"))
    p.add_argument("--workload-class", default=os.environ.get("LI_SIM_WORKLOAD_CLASS", ""))
    p.add_argument("--output-detail", default=None)
    p.add_argument(
        "--format",
        choices=SUMMARY_FORMATS,
        default=os.environ.get("LI_SIM_SUMMARY_FORMAT", "json_min"),
    )
    p.add_argument("-o", "--output", type=Path, default=None)
    p.add_argument("--from-env", action="store_true", help="require LI_SIM_* env vars")
    args = p.parse_args()

    if args.from_env and not os.environ.get("LI_SIM_ALGO_ID"):
        print("LI_SIM_ALGO_ID required with --from-env", file=sys.stderr)
        return 2

    if args.algo_id <= 0:
        print("--algo-id required", file=sys.stderr)
        return 2

    detail = detail_from_env_or_arg(args.output_detail)
    bench = args.benchmark or None
    wl = args.workload_class or None

    summary = build_summary_from_li_run(
        algo_id=args.algo_id,
        ok=bool(args.ok),
        checksum=args.checksum,
        vertical_id=args.vertical_id,
        detail=detail,
        lang=args.lang,
        variant=args.variant,
        workload_class=wl,
        benchmark=bench,
    )

    out = args.output
    if out is None:
        name = summary["benchmark"]
        out = REPO / "benchmarks" / "results" / "li_runs" / f"{name}.li{'' if args.format == 'json' else ''}"
        if args.format == "json":
            out = out.with_suffix(".summary.json") if not str(out).endswith(".json") else out
        elif args.format == "json_min":
            out = REPO / "benchmarks" / "results" / "li_runs" / f"{name}.li.summary.min.json"
        else:
            out = REPO / "benchmarks" / "results" / "li_runs" / f"{name}.li.summary.yaml"

    path = write_summary(out, summary, args.format)
    try:
        print(path.relative_to(REPO))
    except ValueError:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
