#!/usr/bin/env python3
"""Numerical stability stress tests for md_lennard_jones (NVE / harmonic / halving)."""

from __future__ import annotations

import argparse
import csv
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MD_DIR = REPO / "benchmarks" / "tier2_physics" / "md_lennard_jones"
BUILD_DIR = REPO / "build" / "bench" / "md_lennard_jones"
RESULTS = REPO / "benchmarks" / "results"
DEFAULT_OUT = RESULTS / "stability.csv"

STRICT_TESTS = {"harmonic_energy", "momentum_drift"}
ADVISORY_TESTS = {"nve_energy_msd", "timestep_halving_ratio"}
LANGS = ("cpp", "rust", "julia", "li")

CSV_HEADER = [
    "lang",
    "test",
    "metric",
    "value",
    "threshold",
    "passed",
    "reference",
]

REFS = {
    "harmonic_energy": "Swope 1997: qlf=dt^2/2",
    "nve_energy_msd": "Allen-Tildesley: MSD(E/N)<3e-8 @ dt=0.004",
    "timestep_halving_ratio": "Allen-Tildesley: MSD scales dt^4, ratio~16",
    "momentum_drift": "NVE invariant: |P|/N < 1e-8",
}


def build_native_stress(path: Path) -> None:
    cc = os.environ.get("CC", "clang")
    subprocess.check_call(
        [
            cc,
            "-O2",
            "-march=native",
            str(MD_DIR / "cpp" / "md_stress_main.c"),
            str(MD_DIR / "common" / "md_stress.c"),
            "-lm",
            "-o",
            str(path),
        ],
        cwd=REPO,
    )


def build_li_stress(path: Path) -> None:
    lic = REPO / "build" / "compiler" / "lic" / "lic"
    if not lic.is_file():
        raise RuntimeError(f"lic missing at {lic} — run ./scripts/build.sh")
    env = {**os.environ, "LI_EXTRA_C": str(MD_DIR / "common" / "md_stress.c")}
    subprocess.check_call(
        [
            str(lic),
            "build",
            str(MD_DIR / "li" / "stress.li"),
            "-o",
            str(path),
            "--release",
            "-O2",
            "-march=native",
        ],
        cwd=REPO,
        env=env,
    )


def parse_stress_output(stdout: str) -> list[dict[str, object]]:
    lines = [ln for ln in stdout.strip().splitlines() if ln and not ln.startswith("name,")]
    rows: list[dict[str, object]] = []
    for line in lines:
        name, value, threshold, passed = line.split(",")
        rows.append(
            {
                "test": name,
                "metric": name,
                "value": float(value),
                "threshold": float(threshold),
                "passed": bool(int(passed)),
                "reference": REFS.get(name, ""),
            }
        )
    return rows


def run_stress(bin_path: Path) -> list[dict[str, object]]:
    proc = subprocess.run([str(bin_path), "--all"], capture_output=True, text=True)
    if not proc.stdout.strip():
        raise RuntimeError(f"md_stress failed: {proc.stderr}")
    return parse_stress_output(proc.stdout)


def run_li_stress(bin_path: Path) -> list[dict[str, object]]:
    proc = subprocess.run([str(bin_path)], capture_output=True, text=True)
    if not proc.stdout.strip():
        raise RuntimeError(f"li md_stress failed: {proc.stderr}")
    return parse_stress_output(proc.stdout)


def collect_rows() -> list[dict[str, object]]:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    native = BUILD_DIR / "md_stress_native"
    li_bin = BUILD_DIR / "md_stress_li"
    build_native_stress(native)
    build_li_stress(li_bin)

    native_rows = run_stress(native)
    li_rows = run_li_stress(li_bin)

    all_rows: list[dict[str, object]] = []
    for lang in ("cpp", "rust", "julia"):
        for row in native_rows:
            all_rows.append({"lang": lang, **row})
    for row in li_rows:
        all_rows.append({"lang": "li", **row})
    return all_rows


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_HEADER)
        w.writeheader()
        for row in rows:
            w.writerow({k: row[k] for k in CSV_HEADER})


def main() -> int:
    parser = argparse.ArgumentParser(description="MD Lennard-Jones stability stress suite")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--strict", action="store_true", help="exit 1 if any strict test fails")
    args = parser.parse_args()

    rows = collect_rows()
    write_csv(args.out, rows)

    ok = True
    for lang in LANGS:
        lang_rows = [row for row in rows if row["lang"] == lang]
        print(f"\n== {lang} ==")
        for row in lang_rows:
            status = "PASS" if row["passed"] else "FAIL"
            tier = "strict" if row["test"] in STRICT_TESTS else "advisory"
            print(
                f"{status} [{tier}] {row['test']}: {row['value']:.6e} "
                f"(threshold {row['threshold']:.6e}) — {row['reference']}"
            )
            if row["test"] in STRICT_TESTS:
                ok = ok and bool(row["passed"])

    print(f"\nwrote {args.out}")
    if args.strict and not ok:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
