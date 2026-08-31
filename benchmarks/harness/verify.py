#!/usr/bin/env python3
"""Correctness gate for benchmark sources (Tier 0 Li reference smokes).

Tier 0: every `li-tests/benchmarks/tier0_correctness/*.li` must `lic build` with
`--allow-open-vc --no-lean-verify` (matches manifest `verify_open_ok` / `verify_ok`).
Invoked from `bench.py --tier 0` after `run_all.sh`; writes `benchmarks/results/verify.csv`.

Tier 2 native/Li checksum gates live in `bench.py` (`verify_checksum*`), not here.
"""

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

# Wave B tier-2 verify: Lennard-Jones MD + one PDE (heat 2D).
TIER2_SMOKE: tuple[str, ...] = ("md_lennard_jones", "heat_equation_2d")


def lic_build(path: Path) -> bool:
    if not LIC.is_file():
        print(f"lic missing at {LIC}", file=sys.stderr)
        return False
    env = {**os.environ, "LIC": str(LIC)}
    cmd = [str(LIC), "build", "--allow-open-vc", "--no-lean-verify", str(path), "-o", "/dev/null"]
    proc = subprocess.run(
        cmd,
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


def tier2_smoke_verify(
    *,
    write_summary: bool = False,
    output_detail: str = "summary",
    summary_format: str = "json",
) -> list[tuple[str, bool, str]]:
    """Build native+Li checksum parity for md_lennard_jones and heat_equation_2d."""
    if not LIC.is_file():
        print(f"lic missing at {LIC}", file=sys.stderr)
        return [(name, False, "lic missing") for name in TIER2_SMOKE]

    os.environ.setdefault("CC", "clang-22")
    os.environ.setdefault("CXX", "clang++-22")

    sys.path.insert(0, str(REPO / "benchmarks" / "harness"))
    from bench import TIER2_BENCHES, native_result_checksum, verify_benchmark_results, verify_md_refs
    from sim_summary import build_summary, default_summary_path, write_summary

    out: list[tuple[str, bool, str]] = []
    for name in TIER2_SMOKE:
        spec = next(b for b in TIER2_BENCHES if b.name == name)
        build_dir = REPO / "build" / "bench" / spec.name
        build_dir.mkdir(parents=True, exist_ok=True)
        try:
            if name == "md_lennard_jones":
                verify_md_refs()
            verify_benchmark_results(spec, build_dir)
            detail = "checksum ok"
            if write_summary:
                native = build_dir / f"{spec.name}_native"
                verify_line = native_result_checksum(native)
                summary = build_summary(
                    benchmark=name,
                    lang="cpp",
                    variant="reference_native",
                    verify_line=verify_line,
                    passed=True,
                    detail=output_detail,
                )
                path = write_summary(
                    default_summary_path(name, "cpp", summary_format),  # type: ignore[arg-type]
                    summary,
                    summary_format,  # type: ignore[arg-type]
                )
                detail = f"checksum ok; wrote {path.relative_to(REPO)}"
            out.append((name, True, detail))
            print(f"PASS verify tier2 {name}")
        except Exception as exc:
            if write_summary:
                summary = build_summary(
                    benchmark=name,
                    lang="cpp",
                    variant="reference_native",
                    verify_line="0",
                    passed=False,
                    detail=output_detail,
                )
                write_summary(
                    default_summary_path(name, "cpp", summary_format),  # type: ignore[arg-type]
                    summary,
                    summary_format,  # type: ignore[arg-type]
                )
            out.append((name, False, str(exc)))
            print(f"FAIL verify tier2 {name}: {exc}", file=sys.stderr)
    return out


def main() -> int:
    sys.path.insert(0, str(REPO / "benchmarks" / "harness"))
    from sim_summary import SUMMARY_FORMATS

    parser = argparse.ArgumentParser()
    parser.add_argument("--write-csv", type=Path, default=RESULTS / "verify.csv")
    parser.add_argument(
        "--tier0-only",
        action="store_true",
        help="skip tier-2 physics smokes (md_lennard_jones, heat_equation_2d)",
    )
    parser.add_argument(
        "--write-summary",
        action="store_true",
        help="emit benchmarks/results/<bench>/<lang>.summary.json (li_sim_summary_v1)",
    )
    parser.add_argument(
        "--output-detail",
        choices=("summary", "fields", "debug", "repro"),
        default=os.environ.get("LI_SIM_OUTPUT_DETAIL", "summary"),
        help="artifact tier; higher levels add paths in summary JSON (files optional)",
    )
    parser.add_argument(
        "--summary-format",
        choices=SUMMARY_FORMATS,
        default=os.environ.get("LI_SIM_SUMMARY_FORMAT", "json"),
        help="summary serialization: json (pretty), json_min, yaml",
    )
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

    if not args.tier0_only:
        for name, passed, detail in tier2_smoke_verify(
            write_summary=args.write_summary,
            output_detail=args.output_detail,
            summary_format=args.summary_format,
        ):
            ok = ok and passed
            rows.append(
                [
                    name,
                    "li",
                    "verify",
                    1,
                    "checksum",
                    1 if passed else 0,
                    "bool",
                    "",
                    "",
                    detail,
                    passed,
                ]
            )

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
