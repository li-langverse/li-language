#!/usr/bin/env python3
"""tier5_http benchmark harness — reads TOML only (defaults + suite + scenarios)."""

from __future__ import annotations

import argparse
import csv
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from bench_http_parity import check_parity_ratios
from http_bench_servers import (
    pick_free_port,
    resolve_nginx_binary,
    scenario_port,
    start_li_httpd,
    start_nginx,
    stop_proc,
)
from http_bench_toml import (
    REPO,
    TIER5,
    ensure_fixtures,
    list_scenario_names,
    merge_scenario,
    profile_langs,
    profile_timing,
    render_nginx_conf,
    validate_merged,
)

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


def git_sha() -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=REPO,
            text=True,
            stderr=subprocess.DEVNULL,
        )
        return out.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def cpu_model() -> str:
    try:
        with Path("/proc/cpuinfo").open(encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    existing: list[dict[str, object]] = []
    if path.is_file():
        with path.open(newline="") as f:
            for row in csv.DictReader(f):
                existing.append(dict(row))
    by_key = {(r["benchmark"], r["lang"], r["variant"], r["metric"]): r for r in existing}
    for row in rows:
        key = (row["benchmark"], row["lang"], row["variant"], row["metric"])
        by_key[key] = row
    merged = list(by_key.values())
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_HEADER)
        w.writeheader()
        for row in sorted(merged, key=lambda r: (r["benchmark"], r["lang"], r["metric"])):
            w.writerow({k: row.get(k, "") for k in CSV_HEADER})


def wrk_supports_pipeline() -> bool:
    wrk = shutil.which("wrk")
    if not wrk:
        return False
    try:
        proc = subprocess.run([wrk, "--help"], capture_output=True, text=True, timeout=5)
        help_text = (proc.stdout or "") + (proc.stderr or "")
        return "-P" in help_text or "--pipeline" in help_text
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def run_wrk(
    url: str,
    *,
    threads: int,
    connections: int,
    duration_sec: int,
    pipeline: int,
) -> dict[str, float] | None:
    wrk = shutil.which("wrk")
    if not wrk:
        return None
    cmd = [
        wrk,
        f"-t{threads}",
        f"-c{connections}",
        f"-d{duration_sec}s",
        "--latency",
        url,
    ]
    if pipeline > 1 and wrk_supports_pipeline():
        cmd.insert(-2, f"-P{pipeline}")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr or proc.stdout, file=sys.stderr)
        return None
    metrics: dict[str, float] = {}
    for line in (proc.stdout or "").splitlines():
        if "Requests/sec:" in line:
            metrics["rps"] = float(line.split()[-1].replace(",", ""))
        if "99.%" in line and "latency" not in line.lower():
            parts = line.split()
            if len(parts) >= 2:
                try:
                    metrics["p99_latency_ms"] = float(parts[1])
                except ValueError:
                    pass
        if "50.%" in line:
            parts = line.split()
            if len(parts) >= 2:
                try:
                    metrics["p50_latency_ms"] = float(parts[1])
                except ValueError:
                    pass
    return metrics or None


def run_load_for_lang(
    cfg: dict[str, Any],
    lang: str,
    *,
    port: int,
    variant: str,
) -> list[dict[str, object]]:
    load = cfg.get("load") or {}
    tool = str(load.get("tool", "wrk"))
    if tool != "wrk":
        print(f"warn: load tool {tool!r} not implemented", file=sys.stderr)
        return []
    threads = int(load.get("threads", 8))
    connections = int(load.get("connections", 200))
    duration = int(load.get("duration_sec", 30))
    pipeline = int(load.get("pipeline", 1))
    path = "/file.bin"
    verify = cfg.get("verify") or {}
    reqs = verify.get("requests") or []
    if isinstance(reqs, list) and reqs and isinstance(reqs[0], dict):
        path = str(reqs[0].get("path", path))
    url = f"http://127.0.0.1:{port}{path}"
    sweeps = load.get("sweep") or []
    if not isinstance(sweeps, list):
        sweeps = []
    variants: list[tuple[str, int]] = [(variant, connections)]
    for sweep in sweeps:
        if not isinstance(sweep, dict):
            continue
        conns = sweep.get("connections")
        prefix = str(sweep.get("variant_prefix", "c"))
        if isinstance(conns, list):
            for c in conns:
                variants.append((f"{prefix}{c}", int(c)))
    sha = git_sha()
    cpu = cpu_model()
    flags = f"profile={cfg.get('_profile', '')};tool=wrk"
    rows: list[dict[str, object]] = []
    for var, conn in variants:
        metrics = run_wrk(
            url,
            threads=threads,
            connections=conn,
            duration_sec=duration,
            pipeline=pipeline,
        )
        if not metrics:
            continue
        unit_map = {"rps": "req/s", "p50_latency_ms": "ms", "p99_latency_ms": "ms"}
        for metric, value in metrics.items():
            rows.append(
                {
                    "benchmark": cfg["name"],
                    "lang": lang,
                    "variant": var,
                    "threads": threads,
                    "metric": metric,
                    "value": value,
                    "unit": unit_map.get(metric, ""),
                    "git_sha": sha,
                    "cpu_model": cpu,
                    "flags": flags,
                }
            )
    return rows


def dry_run_scenario(name: str, *, profile: str | None, overrides: list[str] | None) -> tuple[bool, str]:
    cfg = merge_scenario(name, overrides=overrides, profile=profile)
    errs = validate_merged(cfg)
    if errs:
        return False, "; ".join(errs)
    ensure_fixtures(cfg)
    if cfg.get("enabled") is False:
        return True, "disabled"
    langs = profile_langs(profile, cfg)
    timing = profile_timing(profile)
    return True, f"merged ok langs={langs} timing={timing}"


def render_nginx_only(name: str, *, profile: str | None, overrides: list[str] | None, out: Path) -> int:
    cfg = merge_scenario(name, overrides=overrides, profile=profile)
    global_tbl = cfg.get("global") or {}
    port_base = int(global_tbl.get("port_base", 18080))
    port = port_base + (hash(name) % 200)
    text = render_nginx_conf(cfg, port=port)
    out.write_text(text, encoding="utf-8")
    print(f"rendered nginx config for {name} -> {out}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="tier5_http bench harness (TOML only)")
    parser.add_argument("scenarios", nargs="*", help="scenario names (default from suite.toml)")
    parser.add_argument("--profile", default=None, help="suite.toml profile (ci, baseline, …)")
    parser.add_argument("--set", action="append", default=[], dest="overrides")
    parser.add_argument("--out", type=Path, default=RESULTS / "latest.csv")
    parser.add_argument("--dry-run", action="store_true", help="merge/validate only, no servers")
    parser.add_argument(
        "--render-nginx-only",
        type=Path,
        metavar="OUT",
        help="write rendered nginx.conf for first scenario and exit",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="run verify_http.py for profile (no timing)",
    )
    parser.add_argument(
        "--check-parity",
        action="store_true",
        help="after timing, require li RPS/latency ratios vs nginx (parity profile)",
    )
    args = parser.parse_args()

    profile = args.profile
    if profile is None:
        suite_path = TIER5 / "suite.toml"
        try:
            from http_bench_toml import load_suite

            suite = load_suite()
            profile = str(suite.get("default_profile", "default"))
        except Exception:
            profile = "default"

    names = list_scenario_names(profile=profile, explicit=args.scenarios or None)
    if not names:
        print("bench_http: no scenarios from suite.toml", file=sys.stderr)
        return 1

    if args.render_nginx_only:
        return render_nginx_only(
            names[0],
            profile=profile,
            overrides=args.overrides,
            out=args.render_nginx_only,
        )

    if args.verify_only:
        verify_script = Path(__file__).with_name("verify_http.py")
        cmd = [sys.executable, str(verify_script), "--profile", profile or "ci", *names]
        for spec in args.overrides:
            cmd.extend(["--set", spec])
        return subprocess.call(cmd)

    if args.dry_run or os.environ.get("TIER5_HTTP_DRY_RUN", "").strip() in ("1", "true", "yes"):
        ok_all = True
        for name in names:
            ok, msg = dry_run_scenario(name, profile=profile, overrides=args.overrides)
            status = "PASS" if ok else "FAIL"
            print(f"{status} bench_http dry-run {name}: {msg}")
            ok_all = ok_all and ok
        return 0 if ok_all else 1

    if not profile_timing(profile):
        verify_script = Path(__file__).with_name("verify_http.py")
        cmd = [
            sys.executable,
            str(verify_script),
            "--profile",
            profile or "ci",
            *names,
        ]
        for spec in args.overrides:
            cmd.extend(["--set", spec])
        if os.environ.get("TIER5_HTTP_STUB", "").strip() in ("1", "true", "yes") or not resolve_nginx_binary():
            cmd.append("--stub")
        return subprocess.call(cmd)

    # Timing path: nginx + li-httpd vs wrk
    verify_script = Path(__file__).with_name("verify_http.py")
    vcmd = [sys.executable, str(verify_script), "--profile", profile or "parity", *names]
    for spec in args.overrides:
        vcmd.extend(["--set", spec])
    if subprocess.call(vcmd) != 0:
        return 1

    if not shutil.which("wrk"):
        print("bench_http: wrk not in PATH — skipping load timing", file=sys.stderr)
        return 0
    if not resolve_nginx_binary():
        print("bench_http: nginx missing — skipping load timing", file=sys.stderr)
        return 0

    all_rows: list[dict[str, object]] = []
    for name in names:
        cfg = merge_scenario(name, overrides=args.overrides, profile=profile)
        cfg["_profile"] = profile or ""
        if cfg.get("enabled") is False:
            continue
        ensure_fixtures(cfg)
        langs = profile_langs(profile, cfg)
        global_tbl = cfg.get("global") or {}
        port_base = int(global_tbl.get("port_base", 18080))
        port = pick_free_port(port_base, sum(ord(c) for c in name) % 200)
        server = cfg.get("server") or {}
        variant = "sendfile_on" if server.get("sendfile", True) else "sendfile_off"
        for lang in langs:
            proc = None
            work_dir: Path | None = None
            try:
                if lang == "nginx":
                    proc, work_dir = start_nginx(cfg, port)
                elif lang == "li":
                    proc, work_dir = start_li_httpd(name, port)
                else:
                    print(f"skip load for {lang!r}", file=sys.stderr)
                    continue
                rows = run_load_for_lang(cfg, lang, port=port, variant=variant)
                all_rows.extend(rows)
            except RuntimeError as exc:
                print(f"bench_http {name} {lang}: {exc}", file=sys.stderr)
                return 1
            finally:
                stop_proc(proc)
                if work_dir and work_dir.exists():
                    shutil.rmtree(work_dir, ignore_errors=True)

    if all_rows:
        write_csv(args.out, all_rows)
        print(f"bench_http: wrote {len(all_rows)} rows -> {args.out}")
    else:
        print("bench_http: no timing rows produced", file=sys.stderr)
        return 1

    if args.check_parity or profile == "parity":
        ok, notes = check_parity_ratios(all_rows, names)
        for line in notes:
            print(line)
        if not ok:
            print("bench_http: parity check FAILED", file=sys.stderr)
            return 1
        print("bench_http: parity check OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
