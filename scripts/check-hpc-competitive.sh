#!/usr/bin/env bash
# Validate benchmarks/competitive/registry.toml and optional latest.csv columns.
# v1: stale last_reviewed warns only (LI_HPC_COMPETITIVE_STRICT=1 → exit 1 on stale).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

REGISTRY="$ROOT/benchmarks/competitive/registry.toml"
CSV="${LI_HPC_COMPETITIVE_CSV:-$ROOT/benchmarks/results/latest.csv}"
STRICT="${LI_HPC_COMPETITIVE_STRICT:-0}"
WARN_DAYS="${LI_HPC_COMPETITIVE_REVIEW_DAYS:-90}"

li_phase "HPC competitive registry"
[[ -f "$REGISTRY" ]] || { li_fail "missing $REGISTRY"; exit 1; }

export REGISTRY CSV WARN_DAYS STRICT
python3 - <<'PY'
from __future__ import annotations

import os
import sys
from datetime import date, datetime
from pathlib import Path

try:
    import tomllib
except ImportError:
    import tomli as tomllib  # type: ignore

registry = Path(os.environ["REGISTRY"])
csv_path = Path(os.environ["CSV"])
warn_days = int(os.environ["WARN_DAYS"])
strict = os.environ.get("STRICT", "0") == "1"

data = tomllib.loads(registry.read_text())
required_eco = ("id", "track", "repo_url", "compare", "last_reviewed", "kernel_honesty")
valid_tracks = {"watch", "bench_tier0", "bench_tier1", "bench_tier2"}
bench_langs: set[str] = set()
errors: list[str] = []
warnings: list[str] = []

ecos = data.get("ecosystem")
if not isinstance(ecos, list) or not ecos:
    errors.append("registry: [[ecosystem]] must be a non-empty array")

ids: set[str] = set()
for i, eco in enumerate(ecos or []):
    if not isinstance(eco, dict):
        errors.append(f"ecosystem[{i}]: not a table")
        continue
    for key in required_eco:
        if key not in eco:
            errors.append(f"{eco.get('id', i)}: missing field {key}")
    eid = eco.get("id", "")
    if eid in ids:
        errors.append(f"duplicate ecosystem id: {eid}")
    ids.add(eid)
    track = eco.get("track", "")
    if track not in valid_tracks:
        errors.append(f"{eid}: invalid track {track!r}")
    if track in ("bench_tier1", "bench_tier2"):
        lang = eco.get("csv_lang", "")
        if not lang:
            errors.append(f"{eid}: {track} requires csv_lang")
        else:
            bench_langs.add(lang)
    compare = eco.get("compare", [])
    if not isinstance(compare, list) or not compare:
        errors.append(f"{eid}: compare must be a non-empty array")
    lr = eco.get("last_reviewed", "")
    try:
        reviewed = date.fromisoformat(str(lr))
        age = (date.today() - reviewed).days
        if age > warn_days:
            msg = f"{eid}: last_reviewed {lr} is {age}d old (warn threshold {warn_days}d)"
            warnings.append(msg)
    except ValueError:
        errors.append(f"{eid}: last_reviewed must be YYYY-MM-DD, got {lr!r}")

if csv_path.is_file():
    import csv

    langs_in_csv: set[str] = set()
    with csv_path.open(newline="") as f:
        reader = csv.DictReader(f)
        if "lang" not in (reader.fieldnames or []):
            errors.append(f"{csv_path}: missing lang column")
        else:
            for row in reader:
                langs_in_csv.add(row.get("lang", ""))
    missing = sorted(bench_langs - langs_in_csv)
    if missing:
        warnings.append(
            f"latest.csv missing lang rows for bench ecosystems: {', '.join(missing)} "
            f"(run: python3 benchmarks/harness/bench.py --tier 12)"
        )
else:
    warnings.append(f"no CSV at {csv_path} — column check skipped")

for w in warnings:
    print(f"warn: {w}", file=sys.stderr)
for e in errors:
    print(f"error: {e}", file=sys.stderr)

if errors:
    sys.exit(1)
if strict and warnings:
    # Scheduled CI may set STRICT=1 to surface stale reviews; v1 default is warn-only.
    stale = [w for w in warnings if "last_reviewed" in w]
    if stale:
        sys.exit(1)
sys.exit(0)
PY
rc=$?
if [[ "$rc" -ne 0 ]]; then
  li_gate_fail "HPC competitive registry"
  exit "$rc"
fi
li_gate_ok "HPC competitive registry"
exit 0
