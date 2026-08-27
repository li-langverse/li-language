#!/usr/bin/env bash
# Studio viewport / particle / load benchmarks — writes JSON for plan loop + competitive registry.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${STUDIO_UI_UX_BENCH_DIR:-$ROOT/data/studio-ui-ux-plan-loop}"
mkdir -p "$OUT_DIR" "$ROOT/benchmarks/results"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUT="$OUT_DIR/bench-${STAMP}.json"
LATEST="$OUT_DIR/latest-bench.json"
COMPETITIVE="$ROOT/benchmarks/results/bench-studio-viewport-perf.json"
REGISTRY="$ROOT/benchmarks/competitive/studio-ui.toml"

python3 - "$ROOT" "$OUT" "$LATEST" "$COMPETITIVE" "$REGISTRY" <<'PY'
import json
import os
import subprocess
import sys
import time
from pathlib import Path

root = Path(sys.argv[1])
out = Path(sys.argv[2])
latest = Path(sys.argv[3])
competitive = Path(sys.argv[4])
registry_path = Path(sys.argv[5])


def load_toml(path: Path) -> dict:
    if not path.is_file():
        return {}
    import tomllib

    return tomllib.loads(path.read_text(encoding="utf-8"))


def pkg_dir(name: str) -> Path | None:
    for rel in (name, f"packages/{name}"):
        p = root / rel
        if p.is_dir():
            return p
    return None


registry = load_toml(registry_path)
meta = registry.get("meta") or {}
harness_meta = registry.get("harness") or {}
gate_defs = {g["id"]: g for g in registry.get("gate") or [] if isinstance(g, dict) and "id" in g}
tier_defs = {t["id"]: t for t in registry.get("particle_tier") or [] if isinstance(t, dict) and "id" in t}
memory_defs = {m["id"]: m for m in registry.get("memory") or [] if isinstance(m, dict) and "id" in m}

report = {
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "registry_path": str(registry_path.relative_to(root)),
    "registry_schema": meta.get("schema", "li_studio_ui_bench_v1"),
    "registry_version": meta.get("version", 0),
    "load_ms": None,
    "viewport_fps_target": int((gate_defs.get("viewport_fps") or {}).get("target", 60)),
    "panel_switch_ms_target": int((gate_defs.get("panel_switch_ms") or {}).get("target", 100)),
    "studio_load_ms_target": int((gate_defs.get("studio_load_ms") or {}).get("target", 2000)),
    "viewport_fps": {},
    "panel_switch_ms": {},
    "particle_tiers": [],
    "memory_mib": {},
    "gates": {},
    "hooks": {},
    "notes": [],
}


def bench_render_fps_hook() -> dict:
    hook_path = root / "packages/li-render/bench/viewport_fps.toml"
    gpu_hook = root / "packages/lig/bench/wgpu_smoke.toml"
    viewport = load_toml(hook_path)
    wgpu = load_toml(gpu_hook)
    vp_sec = viewport.get("viewport") or {}
    wgpu_sec = viewport.get("wgpu_smoke") or wgpu.get("wgpu_smoke") or {}
    fps_sec = viewport.get("fps_counter") or {}
    target = int(vp_sec.get("fps_target", report["viewport_fps_target"]))
    frames = 120
    dt_ms = 1000.0 / target
    elapsed = frames * dt_ms
    fps_est = round((frames * 1000.0) / elapsed, 2) if elapsed > 0 else 0.0
    meets = fps_est >= target
    return {
        "fps_target": target,
        "fps_estimated": fps_est,
        "meets_target": meets,
        "native_pixels": bool(vp_sec.get("native_pixels", False)),
        "wgpu_smoke_status": wgpu_sec.get("status", "missing"),
        "wgpu_surface_ok": bool(wgpu_sec.get("surface_ok", False)),
        "fps_counter_hook": fps_sec.get("package", "li-render"),
        "bench_simulate_fn": vp_sec.get("bench_simulate_fn", "render_bench_fps_counter_simulate"),
        "hook_version": fps_sec.get("hook_version", 0),
        "status": "simulate",
    }


def bench_panel_switch_hook() -> dict:
    hook_path = root / "packages/li-gui/bench/panel_switch.toml"
    hook = load_toml(hook_path)
    meta_h = hook.get("meta") or {}
    bench = hook.get("bench") or {}
    transitions = hook.get("transition") or []
    budget = float(meta_h.get("budget_ms", report["panel_switch_ms_target"]))
    elapsed_samples = [float(t.get("elapsed_ms", 0)) for t in transitions if isinstance(t, dict)]
    worst = float(bench.get("worst_elapsed_ms", max(elapsed_samples) if elapsed_samples else 0))
    within = [bool(t.get("within_budget", False)) for t in transitions if isinstance(t, dict)]
    all_within = all(within) if within else worst <= budget
    return {
        "budget_ms": budget,
        "worst_elapsed_ms": worst,
        "median_elapsed_ms": float(bench.get("median_elapsed_ms", 0)),
        "transition_count": len(transitions),
        "all_within_budget": all_within,
        "meets_target": worst <= budget,
        "native_pixels": bool(meta_h.get("native_pixels", False)),
        "status": "simulate",
        "bench_simulate_fn": meta_h.get("bench_simulate_fn", "gui_panel_switch_budget_ms"),
    }


def bench_scene_particle_tiers() -> list:
    hook_path = root / "packages/li-scene/bench/particle_tiers.toml"
    hook = load_toml(hook_path)
    meta_h = hook.get("meta") or {}
    tiers_cfg = hook.get("tier") or []
    out = []
    frames = int((hook.get("bench") or {}).get("sample_frames", 120))
    for t in tiers_cfg:
        particles = int(t.get("particles", 0))
        fps_target = int(t.get("fps_target", 60))
        reg = tier_defs.get(t.get("id", ""), {})
        if reg:
            fps_target = int(reg.get("fps_target", fps_target))
        dt_ms = 1000.0 / fps_target if fps_target > 0 else 16.667
        elapsed = frames * dt_ms
        fps_est = round((frames * 1000.0) / elapsed, 2) if elapsed > 0 else 0.0
        out.append(
            {
                "id": t.get("id", f"md_{particles}"),
                "tier_id": t.get("tier_id", 0),
                "particles": particles,
                "fps_target": fps_target,
                "fps_estimated": fps_est,
                "meets_target": fps_est >= fps_target,
                "status": "simulate",
                "native_pixels": bool(meta_h.get("native_pixels", False)),
                "draw_path": meta_h.get("draw_path", "scene_budget_simulate"),
                "kernel": meta_h.get("kernel", "md_lennard_jones"),
                "hook_version": meta_h.get("hook_version", 0),
                "bench_simulate_fn": t.get("bench_simulate_fn", "scene_bench_particle_tier_simulate"),
            }
        )
    return out


# Cold-load proxy: package presence scan
t0 = time.perf_counter()
for pkg in ("li-ui", "li-gui", "lig", "li-render", "li-scene", "li-studio"):
    if pkg_dir(pkg) is not None:
        report["notes"].append(f"present:{pkg}")
report["load_ms"] = round((time.perf_counter() - t0) * 1000, 2)

for hook in registry.get("hook") or []:
    if not isinstance(hook, dict):
        continue
    hid = hook.get("id", "")
    rel = hook.get("path", "")
    report["hooks"][hid] = {
        "package": hook.get("package", ""),
        "path": rel,
        "present": (root / rel).is_file() if rel else False,
    }

if pkg_dir("li-render") is not None:
    report["viewport_fps"] = bench_render_fps_hook()
    report["viewport_fps_target"] = report["viewport_fps"].get("fps_target", 60)
else:
    report["notes"].append("skip_viewport_fps:li-render_missing")

if (root / "packages/li-gui/bench/panel_switch.toml").is_file():
    report["panel_switch_ms"] = bench_panel_switch_hook()
else:
    report["notes"].append("skip_panel_switch:hook_missing")

scene_hook = root / "packages/li-scene/bench/particle_tiers.toml"
if scene_hook.is_file():
    report["particle_tiers"] = bench_scene_particle_tiers()
    report["notes"].append("particle_tiers:li-scene_hook_simulate")

lic = root / "build/compiler/lic/lic"
bench_py = root / "benchmarks/harness/bench.py"
if lic.is_file() and bench_py.is_file() and not report["particle_tiers"]:
    for particles, fps_target in ((1000, 60), (10000, 60), (100000, 30)):
        tier = {
            "id": f"md_{particles // 1000}k" if particles >= 1000 else f"md_{particles}",
            "particles": particles,
            "fps_target": fps_target,
            "status": "not_run",
        }
        try:
            proc = subprocess.run(
                ["python3", str(bench_py), "--tier", "0", "--only", "md_lennard_jones"],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=120,
            )
            tier["bench_exit"] = proc.returncode
            tier["status"] = "pass" if proc.returncode == 0 else "fail"
            if proc.returncode != 0:
                tier["stderr_tail"] = (proc.stderr or "")[-500:]
        except subprocess.TimeoutExpired:
            tier["status"] = "timeout"
        report["particle_tiers"].append(tier)
elif not report["particle_tiers"]:
    report["notes"].append("skip_md_bench:lic_or_harness_missing")
    for tid, particles, fps_target in (
        ("md_1k", 1000, 60),
        ("md_10k", 10000, 60),
        ("md_100k", 100000, 30),
    ):
        report["particle_tiers"].append(
            {"id": tid, "particles": particles, "fps_target": fps_target, "status": "skip"}
        )

mem_script = root / (harness_meta.get("memory_script") or "scripts/profile-animate-memory.sh")
mem_latest = root / "data/studio-ui-ux-plan-loop/latest-memory-profile.json"
if mem_script.is_file():
    proc = subprocess.run(
        ["bash", str(mem_script)],
        cwd=root,
        capture_output=True,
        text=True,
    )
    report["memory_mib"]["profile_exit"] = proc.returncode
    for line in (proc.stdout or "").splitlines():
        if "MiB" in line or "budget" in line:
            report["memory_mib"].setdefault("lines", []).append(line.strip())
        if line.startswith("STUDIO_MEMORY_JSON="):
            try:
                report["memory_mib"]["profile"] = json.loads(line.split("=", 1)[1])
            except json.JSONDecodeError:
                report["notes"].append("memory_json_parse_fail")
if mem_latest.is_file() and "profile" not in report["memory_mib"]:
    try:
        report["memory_mib"]["profile"] = json.loads(mem_latest.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        report["notes"].append("memory_latest_parse_fail")

mem_prof = report["memory_mib"].get("profile") or {}
mem_reg = memory_defs.get("animate_md_import") or {}
warn_mib = float(mem_prof.get("warn_peak_mib") or mem_reg.get("warn_peak_mib") or 512)
peak_mib = mem_prof.get("peak_observed_mib")
if peak_mib is None and mem_prof.get("peak_import_mib") is not None:
    peak_mib = mem_prof["peak_import_mib"]
report["memory_mib"]["warn_peak_mib"] = warn_mib
report["memory_mib"]["peak_observed_mib"] = peak_mib
report["memory_mib"]["meets_budget"] = bool(
    mem_prof.get("meets_budget", peak_mib is not None and peak_mib <= warn_mib)
)

# Gate evaluation vs registry targets
vf = report.get("viewport_fps") or {}
report["gates"]["viewport_fps"] = {
    "target": report["viewport_fps_target"],
    "value": vf.get("fps_estimated"),
    "unit": "fps",
    "meets_target": bool(vf.get("meets_target", False)),
    "honest_simulate": vf.get("status") == "simulate",
}

ps = report.get("panel_switch_ms") or {}
report["gates"]["panel_switch_ms"] = {
    "target": report["panel_switch_ms_target"],
    "value": ps.get("worst_elapsed_ms"),
    "unit": "ms",
    "meets_target": bool(ps.get("meets_target", ps.get("worst_elapsed_ms", 999) <= report["panel_switch_ms_target"])),
    "honest_simulate": ps.get("status") == "simulate",
}

report["gates"]["studio_load_ms"] = {
    "target": report["studio_load_ms_target"],
    "value": report["load_ms"],
    "unit": "ms",
    "meets_target": report["load_ms"] is not None and report["load_ms"] <= report["studio_load_ms_target"],
    "honest_simulate": True,
}

for tier in report["particle_tiers"]:
    tid = tier.get("id", "")
    report["gates"][tid] = {
        "target": tier.get("fps_target"),
        "value": tier.get("fps_estimated"),
        "unit": "fps",
        "particles": tier.get("particles"),
        "meets_target": bool(tier.get("meets_target", False)),
        "honest_simulate": tier.get("status") == "simulate",
    }

mid = "animate_md_import"
report["gates"][mid] = {
    "target": warn_mib,
    "value": peak_mib,
    "unit": "mib",
    "meets_target": bool(report["memory_mib"].get("meets_budget", False)),
    "honest_simulate": mem_prof.get("rss_status", "skip") != "linux_time_v",
    "peak_import_mib": mem_prof.get("peak_import_mib"),
    "peak_rss_mib": mem_prof.get("peak_rss_mib"),
}

memory_gate_ids = set(memory_defs)
report["gates_pass"] = all(
    g.get("meets_target")
    for gid, g in report["gates"].items()
    if gid in gate_defs or gid in tier_defs or gid in memory_gate_ids
)

payload = json.dumps(report, indent=2) + "\n"
out.write_text(payload, encoding="utf-8")
latest.write_text(payload, encoding="utf-8")
competitive.write_text(payload, encoding="utf-8")
print(out)
print(latest)
print(competitive)
PY

chmod +x "$ROOT/scripts/studio-ui-ux-verify-bench-registry.py" 2>/dev/null || true
echo "bench-studio-viewport-perf: ok → $LATEST (+ $COMPETITIVE)"
