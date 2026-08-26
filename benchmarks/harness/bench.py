#!/usr/bin/env python3
"""Run benchmark tiers and write results/latest.csv.

Correctness vs timing (do not mix publish rows with unverified kernels):

- **Tier 0:** `run_all.sh` → `verify.py` (`lic build` on tier-0 physics smokes) → `stability.py`.
- **Tier 1–2:** `run_tier_benches(..., verify=True)` runs checksum/native gates *before*
  `run_benchmark` timing sweeps; `--skip-verify` skips only that pre-pass (CI bench timings).
- **Tier 2 CI:** `--tier 2 --ci` → `run_tier2_ci_smoke()` (verify only, no CSV timing).

Contract tests: `benchmarks/harness/test_harness_contract.py`.
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TIER1 = REPO / "benchmarks" / "tier1_micro"
TIER2 = REPO / "benchmarks" / "tier2_physics"
RESULTS = REPO / "benchmarks" / "results"
CSV_HEADER = [
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
]
NATIVE_FLAGS = "-O3 -march=native -ffast-math"
LANGS = ("cpp", "rust", "julia", "li")


@dataclass(frozen=True)
class BenchSpec:
    name: str
    tier: int
    rel_dir: str
    main_c: str
    core_c: str
    li_main: str
    flops_per_run: float | None = None
    bytes_per_run: float | None = None
    li_pure: bool = False


TIER1_BENCHES: tuple[BenchSpec, ...] = (
    BenchSpec(
        "simd_dot",
        1,
        "simd_dot",
        "cpp/main.c",
        "common/dot_core.c",
        "li/main.li",
        flops_per_run=2.0 * 1e7,
        li_pure=False,
    ),
    BenchSpec(
        "matmul_naive",
        1,
        "matmul_naive",
        "cpp/main.c",
        "common/matmul_core.c",
        "li/main.li",
        flops_per_run=2.0 * 256**3,
        li_pure=True,
    ),
    BenchSpec(
        "matmul_blocked",
        1,
        "matmul_blocked",
        "cpp/main.c",
        "common/matmul_blocked_core.c",
        "li/main.li",
        flops_per_run=2.0 * 512**3,
        li_pure=True,
    ),
    BenchSpec(
        "reduce_sum",
        1,
        "reduce_sum",
        "cpp/main.c",
        "common/reduce_core.c",
        "li/main.li",
        bytes_per_run=8.0 * 1e8,
    ),
    BenchSpec(
        "horner_pure_li",
        1,
        "horner_pure_li",
        "cpp/main.c",
        "common/horner_core.c",
        "li/main.li",
        flops_per_run=2.0 * 5e6,
        li_pure=True,
    ),
)

# Gaming-physics roadmap (physics-only; Tier R = rendering out of scope):
#   exists: md_lennard_jones, nbody, wave_1d/2d, heat_2d, advection_diffusion_2d, sph_dam_break_2d (stub)
#   planned: euler_fluid_2d, combustion_passive, wind_field_bc, rigid_body, cloth, mls_mpm
#   Tier R: shadows, reflections BRDF, fire rendering
TIER2_BENCHES: tuple[BenchSpec, ...] = (
    BenchSpec(
        "md_lennard_jones",
        2,
        "md_lennard_jones",
        "cpp/md_main.c",
        "common/md_core.c",
        "li/main.li",
        li_pure=True,
    ),
    BenchSpec(
        "three_body",
        2,
        "three_body",
        "cpp/main.c",
        "common/three_body_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "nbody_gravity",
        2,
        "nbody_gravity",
        "cpp/main.c",
        "common/nbody_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "harmonic_oscillator_chain",
        2,
        "harmonic_oscillator_chain",
        "cpp/main.c",
        "common/harmonic_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "wave_equation_1d",
        2,
        "wave_equation_1d",
        "cpp/main.c",
        "common/wave_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "heat_equation_2d",
        2,
        "heat_equation_2d",
        "cpp/main.c",
        "common/heat_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "double_pendulum",
        2,
        "double_pendulum",
        "cpp/main.c",
        "common/pendulum_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "advection_diffusion_2d",
        2,
        "advection_diffusion_2d",
        "cpp/main.c",
        "common/advdiff_core.c",
        "li/main.li",
    ),
    BenchSpec(
        "wave_equation_2d",
        2,
        "wave_equation_2d",
        "cpp/main.c",
        "common/wave2d_core.c",
        "li/main.li",
    ),
 BenchSpec(
 "sph_dam_break_2d",
 2,
 "sph_dam_break_2d",
 "cpp/main.c",
 "common/sph_dam_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "rigid_body_stack",
 2,
 "rigid_body_stack",
 "cpp/main.c",
 "common/rigid_stack_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "three_body_pure",
 2,
 "three_body_pure",
 "cpp/main.c",
 "common/three_body_core.c",
 "li/main.li",
 li_pure=True,
 ),
 BenchSpec(
 "wind_field_bc",
 2,
 "wind_field_bc",
 "cpp/main.c",
 "common/wind_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "combustion_passive",
 2,
 "combustion_passive",
 "cpp/main.c",
 "common/combust_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "orbit_two_body",
 2,
 "orbit_two_body",
 "cpp/main.c",
 "common/orbit_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "fdtd_waveguide_2d",
 2,
 "fdtd_waveguide_2d",
 "cpp/main.c",
 "common/fdtd_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "schrodinger_1d_barrier",
 2,
 "schrodinger_1d_barrier",
 "cpp/main.c",
 "common/tdse_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "euler_fluid_2d",
 2,
 "euler_fluid_2d",
 "cpp/main.c",
 "common/euler_fluid_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "cloth_swing",
 2,
 "cloth_swing",
 "cpp/main.c",
 "common/cloth_core.c",
 "li/main.li",
 ),
 BenchSpec(
 "ragdoll_chain",
 2,
 "ragdoll_chain",
 "cpp/main.c",
 "common/ragdoll_core.c",
 "li/main.li",
 ),
)


def bench_dir(spec: BenchSpec) -> Path:
    root = TIER1 if spec.tier == 1 else TIER2
    return root / spec.rel_dir


def git_sha() -> str:
    try:
        return (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=REPO)
            .decode()
            .strip()
        )
    except Exception:
        return "dev"


def cpu_model() -> str:
    return platform.processor() or platform.machine()


def write_sample_csv(path: Path) -> None:
    """Placeholder rows until Tier 1–2 binaries exist — drives plot harness."""
    sha = git_sha()
    cpu = cpu_model()
    rows = [
        ("three_body", "li", "release", 1, "wall_time", 0.42, "s", sha, cpu, "-O3"),
        ("three_body", "cpp", "release", 1, "wall_time", 0.38, "s", sha, cpu, "-O3 -march=native"),
        ("three_body", "rust", "release", 1, "wall_time", 0.41, "s", sha, cpu, "--release"),
        ("three_body", "julia", "release", 1, "wall_time", 0.55, "s", sha, cpu, "-O3"),
        ("md_lennard_jones", "li", "release", 1, "wall_time", 1.2, "s", sha, cpu, "-O3"),
        ("md_lennard_jones", "cpp", "release", 1, "wall_time", 1.0, "s", sha, cpu, "-O3"),
        ("md_lennard_jones", "li", "release", 8, "wall_time", 0.22, "s", sha, cpu, "-O3 --threads=8"),
        ("simd_dot", "li", "release", 1, "throughput", 4.8, "GFLOPS", sha, cpu, "simd"),
        ("simd_dot", "cpp", "release", 1, "throughput", 5.1, "GFLOPS", sha, cpu, "simd"),
        ("advection_diffusion_2d", "li", "release", 1, "wall_time", 0.65, "s", sha, cpu, "-O3"),
        ("advection_diffusion_2d", "cpp", "release", 1, "wall_time", 0.60, "s", sha, cpu, "-O3"),
        ("wave_equation_2d", "li", "release", 1, "wall_time", 0.90, "s", sha, cpu, "-O3"),
        ("wave_equation_2d", "cpp", "release", 1, "wall_time", 0.85, "s", sha, cpu, "-O3"),
        ("sph_dam_break_2d", "li", "release", 1, "wall_time", 1.5, "s", sha, cpu, "-O3"),
        ("sph_dam_break_2d", "cpp", "release", 1, "wall_time", 1.4, "s", sha, cpu, "-O3"),
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(CSV_HEADER)
        w.writerows(rows)
    print(f"wrote sample {path} ({len(rows)} rows) — replace with real timings when codegen lands")


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_HEADER)
        w.writeheader()
        for row in rows:
            w.writerow({k: row[k] for k in CSV_HEADER})


def merge_rows(
    existing: list[dict[str, str]],
    new_rows: list[dict[str, object]],
    *,
    benchmark: str,
    langs: set[str] | None = None,
) -> list[dict[str, object]]:
    if langs is None:
        kept = [row for row in existing if row["benchmark"] != benchmark]
    else:
        kept = [
            row
            for row in existing
            if not (row["benchmark"] == benchmark and row["lang"] in langs)
        ]
    return kept + new_rows


def time_command(cmd: list[str], *, cwd: Path | None = None, runs: int = 3) -> float:
    samples: list[float] = []
    # Discard one warmup run (JIT/cache/thermal).
    warmup = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if warmup.returncode != 0:
        raise RuntimeError(
            f"warmup failed ({warmup.returncode}): {' '.join(cmd)}\n"
            f"{warmup.stderr or warmup.stdout}"
        )
    for _ in range(runs):
        start = time.perf_counter()
        proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
        elapsed = time.perf_counter() - start
        if proc.returncode != 0:
            raise RuntimeError(
                f"command failed ({proc.returncode}): {' '.join(cmd)}\n"
                f"{proc.stderr or proc.stdout}"
            )
        samples.append(elapsed)
    return statistics.median(samples)


def build_native(spec: BenchSpec, bin_path: Path) -> None:
    """Shared C perf binary — cpp/rust/julia labels use identical machine code."""
    root = bench_dir(spec)
    main_c = root / spec.main_c
    core = root / spec.core_c
    cc = os.environ.get("CC", "clang")
    subprocess.check_call(
        [cc, "-O3", "-march=native", "-ffast-math", str(main_c), str(core), "-lm", "-o", str(bin_path)],
        cwd=REPO,
    )


def build_li(spec: BenchSpec, bin_path: Path) -> None:
    lic = REPO / "build" / "compiler" / "lic" / "lic"
    if not lic.is_file():
        raise RuntimeError(f"lic missing at {lic} — run ./scripts/build.sh")
    root = bench_dir(spec)
    env = dict(os.environ)
    if not spec.li_pure:
        env["LI_EXTRA_C"] = str(root / spec.core_c)
    subprocess.check_call(
        [
            str(lic),
            "build",
            str(root / spec.li_main),
            "-o",
            str(bin_path),
            "--allow-open-vc",
            "--no-lean-verify",
            "--release",
            "-O3",
            "-ffast-math",
            "-march=native",
        ],
        cwd=REPO,
        env=env,
    )


def horner_reference_acc(*, steps: int = 5_000_000, x: float = 1.1) -> float:
    acc = 0.0
    for _ in range(steps):
        acc = acc * x + 1.0
    return acc


def format_horner_checksum(value: float) -> str:
    """Match C `printf(\"%.17g\\n\", checksum)` used by horner --verify."""
    if math.isinf(value):
        return "inf" if value > 0 else "-inf"
    if math.isnan(value):
        return "nan"
    return f"{value:.17g}"


# Li and native are different programs (e.g. reduced steps); only check native reproducibility.
SKIP_LI_NATIVE_RESULT_PARITY: frozenset[str] = frozenset({"three_body_pure"})


def native_result_checksum(native_bin: Path) -> str:
    return subprocess.check_output([str(native_bin), "--verify"], text=True).strip()


def li_result_checksum(li_bin: Path, *, try_argv_verify: bool) -> str:
    """Read benchmark result via --verify or LI_PRINT_SINK_F64 (last line of stdout)."""
    if try_argv_verify:
        proc = subprocess.run(
            [str(li_bin), "--verify"],
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode == 0 and (proc.stdout or "").strip():
            lines = (proc.stdout or "").strip().splitlines()
            return lines[-1].strip()
    env = {**os.environ, "LI_PRINT_SINK_F64": "1"}
    proc = subprocess.run(
        [str(li_bin)],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"Li result run failed (rc={proc.returncode}, stderr={(proc.stderr or '')!r})"
        )
    lines = (proc.stdout or "").strip().splitlines()
    if not lines:
        raise RuntimeError("Li result run produced no stdout (set li_rt_volatile_sink_f64 on final value)")
    return lines[-1].strip()


def verify_benchmark_results(spec: BenchSpec, build_dir: Path) -> None:
    """Verify results against normative spec (reference.py), then Li vs native when applicable."""
    from reference import (
        TIER1_REFERENCE,
        assert_checksum_against_spec,
        assert_spec_small_matches_table,
    )

    native = build_dir / f"{spec.name}_native"
    li_bin = build_dir / f"{spec.name}_li"
    build_native(spec, native)
    build_li(spec, li_bin)

    ref_case = TIER1_REFERENCE.get(spec.name)
    if ref_case is not None and os.environ.get("BENCH_VERIFY_REFERENCE", "1").strip() not in (
        "0",
        "false",
        "no",
    ):
        assert_spec_small_matches_table(spec.name, ref_case)
        small_expected = format_horner_checksum(ref_case.compute_small())
        print(f"{spec.name} spec small ok: {small_expected}")

    native_out = native_result_checksum(native)
    if ref_case is not None and os.environ.get("BENCH_VERIFY_REFERENCE", "1").strip() not in (
        "0",
        "false",
        "no",
    ):
        assert_checksum_against_spec(
            spec.name,
            native_out,
            label="native",
            size="full",
            ref=ref_case,
            use_small=False,
        )

    if spec.name in SKIP_LI_NATIVE_RESULT_PARITY:
        native_b = build_dir / f"{spec.name}_native_b"
        build_native(spec, native_b)
        native_b_out = native_result_checksum(native_b)
        if native_out != native_b_out:
            raise RuntimeError(f"{spec.name}: native checksum not reproducible")
        print(f"{spec.name} verify ok (native only): checksum={native_out}")
        return

    t0 = time.perf_counter()
    li_out = li_result_checksum(li_bin, try_argv_verify=not spec.li_pure)
    li_elapsed = time.perf_counter() - t0
    if ref_case is not None and os.environ.get("BENCH_VERIFY_REFERENCE", "1").strip() not in (
        "0",
        "false",
        "no",
    ):
        if spec.li_pure and li_elapsed < ref_case.min_li_seconds:
            raise RuntimeError(
                f"{spec.name}: Li ran in {li_elapsed:.4f}s < "
                f"{ref_case.min_li_seconds}s (likely DCE / wrong problem size)"
            )
        assert_checksum_against_spec(
            spec.name,
            li_out,
            label="Li",
            size="full",
            ref=ref_case,
            use_small=False,
        )

    if li_out != native_out:
        raise RuntimeError(
            f"{spec.name}: Li vs native mismatch li={li_out!r} native={native_out!r} "
            "(both should match normative spec; fix codegen or kernel)"
        )

    if os.environ.get("BENCH_VERIFY_TIMING", "").strip() in ("1", "true", "yes"):
        cpp_time = time_command([str(native)], runs=1)
        li_time = time_command([str(li_bin)], runs=1)
        if li_time < cpp_time * 0.45:
            raise RuntimeError(
                f"{spec.name}: suspiciously fast Li ({li_time:.4f}s vs native {cpp_time:.4f}s)"
            )

    variant = "pure Li" if spec.li_pure else "shared C kernel"
    print(f"{spec.name} verify ok ({variant}): result={li_out}")


def verify_checksum(spec: BenchSpec, build_dir: Path) -> None:
    verify_benchmark_results(spec, build_dir)


def verify_md_refs() -> None:
    """Advisory: legacy Julia trace driver may differ from shared native kernel."""
    md = next(b for b in TIER2_BENCHES if b.name == "md_lennard_jones")
    build_dir = REPO / "build" / "bench" / "md_lennard_jones"
    build_dir.mkdir(parents=True, exist_ok=True)
    native_a = build_dir / "md_verify_a"
    native_b = build_dir / "md_verify_b"
    build_native(md, native_a)
    build_native(md, native_b)
    out_a = subprocess.check_output([str(native_a), "--verify"], text=True).strip()
    out_b = subprocess.check_output([str(native_b), "--verify"], text=True).strip()
    if out_a != out_b:
        raise RuntimeError(f"md native kernel not reproducible: {out_a} vs {out_b}")
    julia = shutil.which("julia")
    if julia:
        julia_out = subprocess.check_output(
            [
                julia,
                "--compiled-modules=no",
                str(bench_dir(md) / "julia" / "md_lennard_jones.jl"),
                "--verify",
            ],
            text=True,
        ).strip()
        print(f"md_lennard_jones drift native={out_a} julia_trace={julia_out} (advisory)")
    else:
        print(f"md_lennard_jones verify ok: native drift={out_a}")


def row_for(
    *,
    benchmark: str,
    lang: str,
    value: float,
    metric: str,
    unit: str,
    sha: str,
    cpu: str,
    flags: str,
) -> dict[str, object]:
    return {
        "benchmark": benchmark,
        "lang": lang,
        "variant": "release",
        "threads": 1,
        "metric": metric,
        "value": round(value, 4),
        "unit": unit,
        "git_sha": sha,
        "cpu_model": cpu,
        "flags": flags,
    }


def run_benchmark(spec: BenchSpec, *, runs: int) -> list[dict[str, object]]:
    build_dir = REPO / "build" / "bench" / spec.name
    build_dir.mkdir(parents=True, exist_ok=True)
    sha = git_sha()
    cpu = cpu_model()
    rows: list[dict[str, object]] = []

    cpp_bin = build_dir / f"{spec.name}_cpp"
    rust_bin = build_dir / f"{spec.name}_rust"
    julia_bin = build_dir / f"{spec.name}_julia"
    li_bin = build_dir / f"{spec.name}_li"

    build_native(spec, cpp_bin)
    build_native(spec, rust_bin)
    build_native(spec, julia_bin)
    build_li(spec, li_bin)

    for lang, bin_path, flags in (
        ("cpp", cpp_bin, NATIVE_FLAGS),
        ("rust", rust_bin, f"{NATIVE_FLAGS} (native C kernel)"),
        ("julia", julia_bin, f"{NATIVE_FLAGS} (native C kernel)"),
        ("li", li_bin, f"{'pure lic' if spec.li_pure else 'shared C kernel + lic'} {NATIVE_FLAGS}"),
    ):
        wall = time_command([str(bin_path)], runs=runs)
        rows.append(
            row_for(
                benchmark=spec.name,
                lang=lang,
                value=wall,
                metric="wall_time",
                unit="s",
                sha=sha,
                cpu=cpu,
                flags=flags,
            )
        )
        if spec.flops_per_run is not None and wall > 0:
            gflops = spec.flops_per_run / wall / 1e9
            rows.append(
                row_for(
                    benchmark=spec.name,
                    lang=lang,
                    value=gflops,
                    metric="throughput",
                    unit="GFLOPS",
                    sha=sha,
                    cpu=cpu,
                    flags=flags,
                )
            )
        if spec.bytes_per_run is not None and wall > 0:
            gbps = spec.bytes_per_run / wall / 1e9
            rows.append(
                row_for(
                    benchmark=spec.name,
                    lang=lang,
                    value=gbps,
                    metric="bandwidth",
                    unit="GB/s",
                    sha=sha,
                    cpu=cpu,
                    flags=flags,
                )
            )
        print(f"{spec.name} {lang} wall_time={wall:.4f}s (median of {runs})")

    return rows


def filter_specs(
    specs: tuple[BenchSpec, ...], only: set[str] | None
) -> tuple[BenchSpec, ...]:
    if not only:
        return specs
    filtered = tuple(s for s in specs if s.name in only)
    if not filtered:
        print(f"bench: no benchmarks in scope for --only {sorted(only)}", file=sys.stderr)
    return filtered


def run_tier_benches(
    specs: tuple[BenchSpec, ...], *, runs: int, out: Path, verify: bool, label: str
) -> int:
    if not specs:
        return 1
    if verify:
        for spec in specs:
            build_dir = REPO / "build" / "bench" / spec.name
            build_dir.mkdir(parents=True, exist_ok=True)
            if spec.name == "md_lennard_jones":
                try:
                    verify_md_refs()
                except RuntimeError as exc:
                    print(f"warn: {exc} — continuing with timing", file=sys.stderr)
            try:
                verify_checksum(spec, build_dir)
            except RuntimeError as exc:
                print(f"FAIL verify {spec.name}: {exc}", file=sys.stderr)
                return 1

    merged: list[dict[str, object]] = read_csv(out)
    for spec in specs:
        new_rows = run_benchmark(spec, runs=runs)
        merged = merge_rows(merged, new_rows, benchmark=spec.name)
    write_csv(out, merged)
    names = ", ".join(b.name for b in specs)
    print(f"updated {out} with {label} timings: {names}")
    return 0


def run_tier1_all(
    *, runs: int, out: Path, verify: bool, only: set[str] | None = None
) -> int:
    specs = filter_specs(TIER1_BENCHES, only)
    return run_tier_benches(specs, runs=runs, out=out, verify=verify, label="tier-1")


def run_tier2_all(
    *, runs: int, out: Path, verify: bool, only: set[str] | None = None
) -> int:
    specs = filter_specs(TIER2_BENCHES, only)
    return run_tier_benches(specs, runs=runs, out=out, verify=verify, label="tier-2")


def verify_checksum_match(spec: BenchSpec, build_dir: Path) -> None:
    verify_benchmark_results(spec, build_dir)


def run_verify_results(
    specs: tuple[BenchSpec, ...], *, label: str, only: set[str] | None = None
) -> int:
    specs = filter_specs(specs, only)
    """Verify numerical results only (no timing CSV update)."""
    failures: list[str] = []
    for spec in specs:
        build_dir = REPO / "build" / "bench" / spec.name
        build_dir.mkdir(parents=True, exist_ok=True)
        if spec.name == "md_lennard_jones":
            try:
                verify_md_refs()
            except RuntimeError as exc:
                print(f"warn: {exc}", file=sys.stderr)
        try:
            verify_benchmark_results(spec, build_dir)
        except RuntimeError as exc:
            failures.append(f"{spec.name}: {exc}")
            print(f"FAIL verify {spec.name}: {exc}", file=sys.stderr)
    if failures:
        print(f"result verify: {len(failures)} failure(s) in {label}", file=sys.stderr)
        return 1
    print(f"result verify ok ({label}, {len(specs)} benchmarks)")
    return 0


def run_tier2_ci_smoke(*, only: set[str] | None = None) -> int:
    """Build + checksum verify all Tier-2 kernels (no timing sweep)."""
    specs = filter_specs(TIER2_BENCHES, only)
    for spec in specs:
        build_dir = REPO / "build" / "bench" / spec.name
        build_dir.mkdir(parents=True, exist_ok=True)
        try:
            verify_checksum_match(spec, build_dir)
        except RuntimeError as exc:
            print(f"FAIL {spec.name}: {exc}", file=sys.stderr)
            return 1
    print(f"tier-2 CI smoke ok ({len(specs)} benchmarks)")
    return 0


def run_verify() -> int:
    script = REPO / "benchmarks" / "harness" / "verify.py"
    return subprocess.call([sys.executable, str(script), "--write-csv", str(RESULTS / "verify.csv")])


def run_tier0() -> int:
    script = REPO / "li-tests" / "run_all.sh"
    if not script.exists():
        print("li-tests harness missing", file=sys.stderr)
        return 1
    env = {**os.environ, "LIC": str(REPO / "build" / "compiler" / "lic" / "lic")}
    return subprocess.call([str(script)], cwd=REPO / "li-tests", env=env)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tier", type=int, default=0)
    parser.add_argument("--sample", action="store_true", help="write demo CSV for plots")
    parser.add_argument("--ci", action="store_true")
    parser.add_argument("--runs", type=int, default=3, help="timing repetitions (median)")
    parser.add_argument("--skip-verify", action="store_true")
    parser.add_argument(
        "--verify-results",
        action="store_true",
        help="verify numerical results only (no timing CSV); tier 1, 2, or 12",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=RESULTS / "latest.csv",
        help="CSV output path",
    )
    parser.add_argument(
        "--only",
        default="",
        help="comma-separated benchmark ids (subset of tier list)",
    )
    parser.add_argument(
        "--package",
        action="append",
        default=[],
        help="workspace package(s) from benchmarks/manifest.toml",
    )
    parser.add_argument(
        "--changed",
        action="store_true",
        help="scope benches to packages touched in git worktree",
    )
    args = parser.parse_args()

    only: set[str] | None = None
    if args.only.strip():
        only = {x.strip() for x in args.only.split(",") if x.strip()}
    elif args.package or args.changed:
        sys.path.insert(0, str(REPO / "benchmarks" / "harness"))
        from bench_scope import resolve_scope

        scope = resolve_scope(packages=args.package or None, changed=args.changed)
        if scope["benches"]:
            only = set(scope["benches"])
        else:
            print("bench: no benches for scope (nothing to time)", file=sys.stderr)
            return 0

    if args.sample:
        write_sample_csv(args.out)
        return 0

    if args.tier == 0 and not (args.out).exists():
        write_sample_csv(args.out)

    if args.ci and args.tier == 2:
        return run_tier2_ci_smoke(only=only)

    if args.verify_results:
        if args.tier in (1, 12):
            rc = run_verify_results(TIER1_BENCHES, label="tier-1", only=only)
            if rc != 0:
                return rc
        if args.tier in (2, 12):
            return run_verify_results(TIER2_BENCHES, label="tier-2", only=only)
        if args.tier == 0:
            print("verify-results: use --tier 1, 2, or 12", file=sys.stderr)
            return 1
        print(f"verify-results: use --tier 1, 2, or 12 (got {args.tier})", file=sys.stderr)
        return 1

    if args.tier == 0:
        rc = run_tier0()
        if rc != 0:
            return rc
        rc = run_verify()
        if rc != 0:
            return rc
        return subprocess.call([sys.executable, str(REPO / "benchmarks" / "harness" / "stability.py")])

    if args.tier == 1:
        return run_tier1_all(
            runs=args.runs, out=args.out, verify=not args.skip_verify, only=only
        )

    if args.tier == 2:
        return run_tier2_all(
            runs=args.runs, out=args.out, verify=not args.skip_verify, only=only
        )

    if args.tier == 12:
        rc = run_tier1_all(
            runs=args.runs, out=args.out, verify=not args.skip_verify, only=only
        )
        if rc != 0:
            return rc
        return run_tier2_all(
            runs=args.runs, out=args.out, verify=not args.skip_verify, only=only
        )

    if args.tier == 3:
        print(
            "tier 3 is HTTP (run from li-langverse/benchmarks: run-tier5-http-bench.sh)",
            file=sys.stderr,
        )
        return 0

    if args.tier == 5:
        script = REPO / "benchmarks" / "harness" / "bench_ecosystem.py"
        return subprocess.call([sys.executable, str(script), "--runs", str(args.runs)])

    if args.tier >= 1:
        print(f"tier {args.tier} benchmarks: not implemented", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
