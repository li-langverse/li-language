#!/usr/bin/env bash
# Peak memory for animate_md.py — tracemalloc import + optional RSS short run.
# Writes data/studio-ui-ux-plan-loop/latest-memory-profile.json (plan loop gates + bench).
#
# animate_md.py streams trajectories; matplotlib 3D GIFs remain RAM-heavy at export.
# Usage: ./scripts/profile-animate-memory.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${STUDIO_UI_UX_BENCH_DIR:-$ROOT/data/studio-ui-ux-plan-loop}"
mkdir -p "$OUT_DIR"
LATEST_JSON="$OUT_DIR/latest-memory-profile.json"
REGISTRY="$ROOT/benchmarks/competitive/studio-ui.toml"

python3 - "$ROOT" "$LATEST_JSON" "$REGISTRY" <<'PY'
from __future__ import annotations

import json
import re
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

root = Path(sys.argv[1])
latest = Path(sys.argv[2])
registry_path = Path(sys.argv[3])

warn_peak_mib = 512.0
memory_id = "animate_md_import"
if registry_path.is_file():
    import tomllib

    reg = tomllib.loads(registry_path.read_text(encoding="utf-8"))
    for block in reg.get("memory") or []:
        if isinstance(block, dict) and block.get("id") == memory_id:
            warn_peak_mib = float(block.get("warn_peak_mib", warn_peak_mib))
            memory_id = block.get("id", memory_id)
            break

harness_dir = root / "benchmarks/harness"
harness_py = harness_dir / "animate_md.py"

# --- tracemalloc peak on import ---
sys.path.insert(0, str(harness_dir))
tracemalloc.start()
import animate_md  # noqa: F401

_, peak_import = tracemalloc.get_traced_memory()
tracemalloc.stop()
peak_import_mib = round(peak_import / 1048576, 2)

# --- optional RSS short run (Linux /usr/bin/time -v or macOS time -l) ---
peak_rss_mib: float | None = None
rss_status = "skip"
lic = root / "build/compiler/lic/lic"
if harness_py.is_file():
    cmd = [
        sys.executable,
        str(harness_py),
        "--skip-export",
        "--max-frames",
        "4",
    ]
    uname = subprocess.run(["uname", "-s"], capture_output=True, text=True).stdout.strip()
    if uname == "Linux":
        time_bin = Path("/usr/bin/time")
        if not time_bin.is_file():
            which = subprocess.run(["which", "time"], capture_output=True, text=True)
            candidate = (which.stdout or "").strip()
            if candidate:
                time_bin = Path(candidate)
        if time_bin.is_file():
            rss_status = "linux_time_v"
            proc = subprocess.run(
                [str(time_bin), "-v", *cmd],
                cwd=root,
                capture_output=True,
                text=True,
            )
            m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", proc.stderr or "")
            if m:
                peak_rss_mib = round(int(m.group(1)) / 1024, 2)
    elif uname == "Darwin" and Path("/usr/bin/time").is_file():
        if lic.is_file():
            rss_status = "darwin_time_l"
            proc = subprocess.run(
                ["/usr/bin/time", "-l", *cmd],
                cwd=root,
                capture_output=True,
                text=True,
            )
            m = re.search(r"(\d+)\s+maximum resident set size", proc.stderr or "")
            if m:
                peak_rss_mib = round(int(m.group(1)) / 1048576, 2)
        else:
            rss_status = "skip_lic_not_built"

peak_observed_mib = peak_rss_mib if peak_rss_mib is not None else peak_import_mib
meets_budget = peak_observed_mib <= warn_peak_mib

doc = {
    "schema": "li_studio_memory_profile_v1",
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "memory_id": memory_id,
    "warn_peak_mib": warn_peak_mib,
    "peak_import_mib": peak_import_mib,
    "peak_rss_mib": peak_rss_mib,
    "peak_observed_mib": peak_observed_mib,
    "meets_budget": meets_budget,
    "rss_status": rss_status,
    "registry_path": str(registry_path.relative_to(root)),
    "notes": [
        "import peak = tracemalloc after loading animate_md",
        "rss peak = --skip-export --max-frames 4 when /usr/bin/time available",
        "full GIF export can exceed budget; Studio timeline uses streamed frames",
    ],
}

latest.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")

print(f"==> tracemalloc peak (import animate_md)")
print(f"    tracemalloc peak (import): {peak_import_mib:.2f} MiB")
if peak_rss_mib is not None:
    print(f"==> short run RSS ({rss_status})")
    print(f"    peak RSS: {peak_rss_mib:.2f} MiB")
else:
    print(f"==> short run RSS: skipped ({rss_status})")
print(f"==> budget warn_peak_mib={warn_peak_mib:.0f} observed={peak_observed_mib:.2f} meets={meets_budget}")
print(f"STUDIO_MEMORY_JSON={json.dumps(doc, separators=(',', ':'))}")
print(f"profile-animate-memory: ok → {latest}")
if not meets_budget:
    sys.exit(1)
PY
