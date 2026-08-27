#!/usr/bin/env python3
"""Package-scoped sim verify: composable build, summaries, selective tier-2, registry."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HARNESS = REPO / "benchmarks" / "harness"
REGISTRY = REPO / "benchmarks" / "competitive" / "algo_registry.json"


def run(cmd: list[str], *, cwd: Path | None = None) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=cwd or REPO, env={**os.environ})


def hook_algo_registry() -> int:
    doc = json.loads(REGISTRY.read_text())
    count = int(doc.get("count", 0))
    algos = doc.get("algorithms", [])
    if count != len(algos) or count < 1:
        print(f"algo_registry: bad count={count} len={len(algos)}", file=sys.stderr)
        return 1
    print(f"algo_registry: ok count={count}")
    return 0


def hook_sim_summary() -> int:
    script = REPO / "li-tests" / "tooling" / "sim_li_run_summary.sh"
    if not script.is_file():
        print("sim_summary: missing sim_li_run_summary.sh", file=sys.stderr)
        return 1
    rc = run(["bash", str(script)])
    if rc != 0:
        return rc
    return run(["bash", str(REPO / "scripts" / "validate-sim-summary.sh")])


def build_composable(path: str, lic: Path) -> int:
    full = REPO / "li-tests" / path
    if not full.is_file():
        print(f"composable missing: {path}", file=sys.stderr)
        return 1
    return run(
        [
            str(lic),
            "build",
            "--allow-open-vc",
            "--no-lean-verify",
            str(full),
            "-o",
            "/dev/null",
        ]
    )


def tier2_verify_benches(names: list[str], *, write_summary: bool) -> int:
    if not names:
        return 0
    smoke = ("md_lennard_jones", "heat_equation_2d")
    run_smoke = [b for b in names if b in smoke]
    if not run_smoke:
        print(f"tier2 verify: skip (no smokes in scope: {names})")
        return 0
    cmd = [sys.executable, str(HARNESS / "verify.py"), "--tier0-only"]
    if write_summary:
        fmt = os.environ.get("LI_SIM_SUMMARY_FORMAT", "json_min")
        cmd.extend(["--write-summary", "--summary-format", fmt])
    # verify.py tier2 runs all TIER2_SMOKE — acceptable for md+heat package scope
    return run(cmd)


def main() -> int:
    sys.path.insert(0, str(HARNESS))
    from bench_scope import resolve_scope  # noqa: E402

    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--package", action="append", default=[])
    p.add_argument("--changed", action="store_true")
    p.add_argument("--write-summary", action="store_true")
    p.add_argument("--skip-tier2", action="store_true")
    args = p.parse_args()

    if not args.package and not args.changed:
        args.package = ["li-sim-scientific"]

    scope = resolve_scope(packages=args.package, changed=args.changed)
    lic = Path(os.environ.get("LIC", REPO / "build" / "compiler" / "lic" / "lic"))
    if not lic.is_file():
        print(f"bench_sim: lic missing at {lic}", file=sys.stderr)
        return 1

    rc = 0
    for hook in scope["hooks"]:
        if hook == "algo_registry":
            rc = hook_algo_registry() or rc
        elif hook == "sim_summary":
            rc = hook_sim_summary() or rc

    for comp in scope["composable"]:
        rc = build_composable(comp, lic) or rc

    if not args.skip_tier2:
        rc = tier2_verify_benches(scope["benches"], write_summary=args.write_summary) or rc

    if rc != 0:
        print("bench_sim: FAIL", file=sys.stderr)
    else:
        print(
            f"bench_sim: ok packages={scope['packages']} benches={scope['benches']}"
        )
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
