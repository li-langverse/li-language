#!/usr/bin/env python3
"""Tier-5 ecosystem benchmarks (deferred): compile time, security gates, async/httpd prep.

Writes rows compatible with li-langverse/benchmarks ingest (latest.csv + security.csv).
"""

from __future__ import annotations

import argparse
import csv
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RESULTS = REPO / "benchmarks" / "results"
LIC = REPO / "build" / "compiler" / "lic" / "lic"

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

SECURITY_HEADER = ["lang", "test", "metric", "value", "threshold", "passed", "reference"]

COMPILE_FIXTURES: tuple[tuple[str, Path], ...] = (
    ("lic_build_async", REPO / "li-tests/async/await_codegen_ok.li"),
    ("lic_build_effects_net", REPO / "li-tests/effects/net_ok.li"),
    ("lic_build_effects_async", REPO / "li-tests/effects/async_ok.li"),
    ("lic_build_alloc", REPO / "li-tests/effects/alloc_ok.li"),
    ("lic_check_contracts", REPO / "li-tests/contracts_verify/discharge_trivial.li"),
)

SECURITY_SCRIPTS: tuple[tuple[str, Path], ...] = (
    ("security_corpus", REPO / "li-tests/run_security.sh"),
    ("security_cve_patterns", REPO / "scripts/check-cve-coverage.sh"),
    ("security_webserver_registry", REPO / "scripts/check-webserver-bugs.sh"),
)


def git_sha() -> str:
    try:
        return (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, text=True)
            .strip()
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def cpu_model() -> str:
    if sys.platform == "darwin":
        try:
            return subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
        except subprocess.CalledProcessError:
            pass
    return platform.processor() or "unknown"


def time_command(cmd: list[str], *, runs: int, cwd: Path | None = None) -> float:
    samples: list[float] = []
    for _ in range(max(1, runs)):
        t0 = time.perf_counter()
        subprocess.run(cmd, cwd=cwd or REPO, check=True, capture_output=True)
        samples.append(time.perf_counter() - t0)
    return statistics.median(samples)


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
    variant: str = "release",
) -> dict[str, object]:
    return {
        "benchmark": benchmark,
        "lang": lang,
        "variant": variant,
        "threads": 1,
        "metric": metric,
        "value": round(value, 4),
        "unit": unit,
        "git_sha": sha,
        "cpu_model": cpu,
        "flags": flags,
    }


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, object]], header: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=header)
        w.writeheader()
        for row in rows:
            w.writerow({k: row.get(k, "") for k in header})


def merge_latest(existing: list[dict[str, str]], new_rows: list[dict[str, object]]) -> list[dict[str, object]]:
    drop = {str(r["benchmark"]) for r in new_rows}
    kept = [dict(r) for r in existing if r.get("benchmark") not in drop]
    return kept + new_rows


def bench_compile(*, runs: int, sha: str, cpu: str) -> list[dict[str, object]]:
    if not LIC.is_file():
        raise RuntimeError(f"lic missing at {LIC} — run ./scripts/build.sh")
    rows: list[dict[str, object]] = []
    for bench_id, src in COMPILE_FIXTURES:
        wall = time_command([str(LIC), "build", str(src), "-o", os.devnull, "--release"], runs=runs)
        rows.append(
            row_for(
                benchmark=bench_id,
                lang="lic",
                value=wall,
                metric="wall_time",
                unit="s",
                sha=sha,
                cpu=cpu,
                flags="lic build --release",
            )
        )
        print(f"{bench_id} lic build wall_time={wall:.4f}s")
    return rows


def bench_async_runtime(*, runs: int, sha: str, cpu: str) -> list[dict[str, object]]:
    spec_dir = REPO / "benchmarks/tier3_ecosystem/async_await_chain"
    li_main = spec_dir / "li/main.li"
    if not li_main.is_file():
        return []
    bin_path = REPO / "build/bench/async_await_chain_li"
    bin_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.check_call(
        [str(LIC), "build", str(li_main), "-o", str(bin_path), "--release"],
        cwd=REPO,
    )
    wall = time_command([str(bin_path)], runs=runs)
    print(f"async_await_chain li wall_time={wall:.4f}s")
    return [
        row_for(
            benchmark="async_await_chain",
            lang="li",
            value=wall,
            metric="wall_time",
            unit="s",
            sha=sha,
            cpu=cpu,
            flags="async await chain d10 (epoll/kqueue reactor)",
        )
    ]


def bench_li_tests_smoke(*, sha: str, cpu: str) -> list[dict[str, object]]:
    wall = time_command(["./run_all.sh"], runs=1, cwd=REPO / "li-tests")
    print(f"li_tests_full harness wall_time={wall:.4f}s")
    return [
        row_for(
            benchmark="li_tests_full",
            lang="harness",
            value=wall,
            metric="wall_time",
            unit="s",
            sha=sha,
            cpu=cpu,
            flags="li-tests/run_all.sh",
        )
    ]


def bench_security(*, runs: int, sha: str, cpu: str) -> tuple[list[dict[str, object]], list[dict[str, str]]]:
    """Wall time for security scripts + pass rows for dashboard stability-style chart."""
    perf_rows: list[dict[str, object]] = []
    sec_rows: list[dict[str, str]] = []
    for test_id, script in SECURITY_SCRIPTS:
        if not script.is_file():
            continue
        wall = time_command(["bash", str(script)], runs=runs)
        perf_rows.append(
            row_for(
                benchmark=f"security_{test_id}",
                lang="harness",
                value=wall,
                metric="wall_time",
                unit="s",
                sha=sha,
                cpu=cpu,
                flags=str(script.relative_to(REPO)),
                variant="ci",
            )
        )
        sec_rows.append(
            {
                "lang": "harness",
                "test": test_id,
                "metric": "wall_time",
                "value": str(round(wall, 4)),
                "threshold": "0",
                "passed": "true",
                "reference": "pass",
            }
        )
        print(f"security_{test_id} wall_time={wall:.4f}s")
    # Full ci-security aggregate
    sec_script = REPO / "scripts/ci-security.sh"
    if sec_script.is_file():
        wall = time_command(["bash", str(sec_script)], runs=1)
        perf_rows.append(
            row_for(
                benchmark="security_gate_full",
                lang="harness",
                value=wall,
                metric="wall_time",
                unit="s",
                sha=sha,
                cpu=cpu,
                flags="scripts/ci-security.sh",
                variant="ci",
            )
        )
        sec_rows.append(
            {
                "lang": "harness",
                "test": "ci_security_full",
                "metric": "wall_time",
                "value": str(round(wall, 4)),
                "threshold": "0",
                "passed": "true",
                "reference": "pass",
            }
        )
    return perf_rows, sec_rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Tier-3 ecosystem benchmarks for dashboard ingest")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--latest", type=Path, default=RESULTS / "latest.csv")
    parser.add_argument("--security", type=Path, default=RESULTS / "security.csv")
    parser.add_argument("--skip-runtime", action="store_true")
    parser.add_argument("--skip-security", action="store_true")
    parser.add_argument(
        "--with-li-tests",
        action="store_true",
        help="include full li-tests/run_all.sh timing (slow; may fail on WIP)",
    )
    args = parser.parse_args()

    if not LIC.is_file():
        print(f"error: build lic first ({LIC})", file=sys.stderr)
        return 1

    sha = git_sha()
    cpu = cpu_model()
    new_rows: list[dict[str, object]] = []

    new_rows.extend(bench_compile(runs=args.runs, sha=sha, cpu=cpu))
    if not args.skip_runtime:
        new_rows.extend(bench_async_runtime(runs=args.runs, sha=sha, cpu=cpu))
    if args.with_li_tests:
        try:
            new_rows.extend(bench_li_tests_smoke(sha=sha, cpu=cpu))
        except subprocess.CalledProcessError as exc:
            print(f"warn: li_tests_full skipped ({exc})", file=sys.stderr)

    merged = merge_latest(read_csv(args.latest), new_rows)
    write_csv(args.latest, merged, CSV_HEADER)

    if not args.skip_security:
        sec_perf, sec_rows = bench_security(runs=1, sha=sha, cpu=cpu)
        merged = merge_latest(read_csv(args.latest), sec_perf)
        write_csv(args.latest, merged, CSV_HEADER)
        write_csv(args.security, sec_rows, SECURITY_HEADER)

    print(f"updated {args.latest} (+{len(new_rows)} ecosystem rows)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
