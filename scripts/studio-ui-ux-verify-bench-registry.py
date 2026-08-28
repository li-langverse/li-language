#!/usr/bin/env python3
"""Validate benchmarks/competitive/studio-ui.toml and bench JSON outputs."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "benchmarks/competitive/studio-ui.toml"
LATEST = ROOT / "data/studio-ui-ux-plan-loop/latest-bench.json"
COMPETITIVE = ROOT / "benchmarks/results/bench-studio-viewport-perf.json"


def fail(msg: str) -> None:
    print(f"studio-ui-ux-verify-bench-registry: {msg}", file=sys.stderr)
    sys.exit(1)


def load_toml(path: Path) -> dict:
    import tomllib

    return tomllib.loads(path.read_text(encoding="utf-8"))


def main() -> None:
    if not REGISTRY.is_file():
        fail(f"missing {REGISTRY.relative_to(ROOT)}")

    reg = load_toml(REGISTRY)
    meta = reg.get("meta") or {}
    if not meta.get("schema"):
        fail("meta.schema required")
    harness = reg.get("harness") or {}
    for key in ("script", "output_latest", "output_competitive"):
        if key not in harness:
            fail(f"harness.{key} required")

    gate_ids = {g["id"] for g in reg.get("gate") or [] if isinstance(g, dict) and "id" in g}
    for required in ("viewport_fps", "panel_switch_ms", "studio_load_ms"):
        if required not in gate_ids:
            fail(f"missing [[gate]] id={required}")

    tier_ids = {t["id"] for t in reg.get("particle_tier") or [] if isinstance(t, dict) and "id" in t}
    for required in ("md_1k", "md_10k", "md_100k"):
        if required not in tier_ids:
            fail(f"missing [[particle_tier]] id={required}")

    memory_ids = {m["id"] for m in reg.get("memory") or [] if isinstance(m, dict) and "id" in m}
    if "animate_md_import" not in memory_ids:
        fail("missing [[memory]] id=animate_md_import")
    mem_script = (reg.get("harness") or {}).get("memory_script", "")
    if mem_script and not (ROOT / mem_script).is_file():
        fail(f"harness.memory_script missing: {mem_script}")
    mem_latest = ROOT / "data/studio-ui-ux-plan-loop/latest-memory-profile.json"
    if not mem_latest.is_file():
        fail("missing latest-memory-profile.json — run ./scripts/profile-animate-memory.sh")

    for hook in reg.get("hook") or []:
        if not isinstance(hook, dict):
            continue
        rel = hook.get("path", "")
        if rel and not (ROOT / rel).is_file():
            fail(f"hook {hook.get('id', '?')}: missing path {rel}")

    for path, label in ((LATEST, "latest-bench"), (COMPETITIVE, "competitive")):
        if not path.is_file():
            fail(f"missing {label} — run ./scripts/bench-studio-viewport-perf.sh")
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("registry_schema") != meta.get("schema"):
            fail(f"{label}: registry_schema mismatch")
        gates = data.get("gates") or {}
        for gid in ("viewport_fps", "panel_switch_ms", "studio_load_ms"):
            if gid not in gates:
                fail(f"{label}: gates.{gid} missing")
        if "particle_tiers" not in data or not data["particle_tiers"]:
            fail(f"{label}: particle_tiers empty")
        if data.get("load_ms") is None:
            fail(f"{label}: load_ms missing")
        mem = data.get("memory_mib") or {}
        if mem.get("peak_observed_mib") is None and not (mem.get("profile") or {}).get("peak_observed_mib"):
            fail(f"{label}: memory_mib.peak_observed_mib missing")
        if "animate_md_import" not in gates:
            fail(f"{label}: gates.animate_md_import missing")
        mg = gates.get("animate_md_import") or {}
        if mg.get("unit") != "mib":
            fail(f"{label}: gates.animate_md_import.unit must be mib")

    print(
        "studio-ui-ux-verify-bench-registry: ok "
        f"(registry v{meta.get('version', '?')}, gates={len(gate_ids)}, tiers={len(tier_ids)}, "
        f"memory={len(memory_ids)})"
    )


if __name__ == "__main__":
    main()
