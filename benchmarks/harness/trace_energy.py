#!/usr/bin/env python3
"""Run md_lennard_jones reference sims with --trace for energy CSVs."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MD_DIR = REPO / "benchmarks" / "tier2_physics" / "md_lennard_jones"
BUILD_DIR = REPO / "build" / "bench" / "md_lennard_jones"
DEFAULT_OUT = REPO / "benchmarks" / "results" / "md_lennard_jones"


def build_cpp(bin_path: Path) -> None:
    build_native(bin_path, main_c="cpp/md_main.c")


def build_rust(bin_path: Path) -> None:
    cargo = shutil.which("cargo")
    if not cargo:
        raise RuntimeError("cargo not found")
    subprocess.check_call([cargo, "build", "--release", "--quiet"], cwd=MD_DIR / "rust")
    built = MD_DIR / "rust" / "target" / "release" / "md_lennard_jones"
    shutil.copy2(built, bin_path)


def build_native(bin_path: Path, *, main_c: str = "cpp/md_main.c") -> None:
    cc = os.environ.get("CC", "clang")
    subprocess.check_call(
        [
            cc,
            "-O3",
            "-march=native",
            "-ffast-math",
            str(MD_DIR / main_c),
            str(MD_DIR / "common" / "md_core.c"),
            "-lm",
            "-o",
            str(bin_path),
        ],
        cwd=REPO,
    )


def build_li_trace(bin_path: Path) -> None:
    lic = REPO / "build" / "compiler" / "lic" / "lic"
    if not lic.is_file():
        raise RuntimeError(f"lic missing at {lic} — run ./scripts/build.sh")
    extra = f"{MD_DIR / 'common' / 'md_core.c'} {MD_DIR / 'common' / 'md_trace_env.c'}"
    env = {**os.environ, "LI_EXTRA_C": extra}
    subprocess.check_call(
        [
            str(lic),
            "build",
            str(MD_DIR / "li" / "trace.li"),
            "-o",
            str(bin_path),
            "--release",
            "-O3",
            "-march=native",
            "-ffast-math",
        ],
        cwd=REPO,
        env=env,
    )


def run_traces(out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    cpp_bin = BUILD_DIR / "md_lj_cpp"
    rust_bin = BUILD_DIR / "md_lj_rust"
    li_bin = BUILD_DIR / "md_lj_li"
    build_cpp(cpp_bin)
    build_rust(rust_bin)
    build_li_trace(li_bin)

    julia = shutil.which("julia")
    if not julia:
        raise RuntimeError("julia not found")

    specs = [
        ("cpp", [str(cpp_bin)]),
        ("rust", [str(rust_bin)]),
        ("julia", [julia, "--compiled-modules=no", str(MD_DIR / "julia" / "md_lennard_jones.jl")]),
        ("li", [str(li_bin)]),
    ]

    for lang, cmd in specs:
        trace_path = out_dir / f"energy_{lang}.csv"
        env = None
        if lang == "li":
            env = {**os.environ, "LI_MD_TRACE": str(trace_path.resolve())}
        subprocess.check_call([*cmd], cwd=REPO, env=env)
        print(f"wrote {trace_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate MD Lennard-Jones energy traces")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    run_traces(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
