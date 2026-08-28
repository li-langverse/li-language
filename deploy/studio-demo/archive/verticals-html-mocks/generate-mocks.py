#!/usr/bin/env python3
"""Generate per-profile Studio marketing HTML mocks (UX-14 honesty banners)."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PROFILES: list[dict[str, str]] = [
    {
        "id": "game",
        "chip": "Game",
        "journey": "default_shell_selection_timeline",
        "viewport_label": "",
        "agent_line": "Idle · ready to author",
        "agent_context": "",
        "inspector": "selection",
        "hud_extra": "Selection ring · timeline playhead at 35%",
        "show_timeline": "1",
        "show_selection": "1",
    },
    {
        "id": "sim_rl",
        "chip": "RL training",
        "journey": "profile_chip_agent_training_env",
        "viewport_label": "Env viewport · step 12,420",
        "agent_line": "Running · policy rollout batch 8",
        "agent_context": "Context: training env",
        "inspector": "rl",
        "hud_extra": "Reward: +1.24 · episode 42",
        "show_timeline": "0",
        "show_selection": "0",
    },
    {
        "id": "sim_automotive",
        "chip": "Automotive",
        "journey": "profile_viewport_placeholder",
        "viewport_label": "PLACEHOLDER — driving scene / sensor rig (not loaded)",
        "agent_line": "Idle · map + vehicle stack pending",
        "agent_context": "",
        "inspector": "auto",
        "hud_extra": "Lane model: stub · li-sim-automotive",
        "show_timeline": "0",
        "show_selection": "0",
    },
    {
        "id": "sim_robotics",
        "chip": "Robotics",
        "journey": "profile_inspector_fields",
        "viewport_label": "Cell layout · UR5 stub",
        "agent_line": "Idle · IK solver not wired",
        "agent_context": "",
        "inspector": "robotics",
        "hud_extra": "Joint targets: compose-only",
        "show_timeline": "0",
        "show_selection": "1",
    },
    {
        "id": "sim_additive",
        "chip": "Additive",
        "journey": "profile_export_hint",
        "viewport_label": "Build volume · layer 128 / 240",
        "agent_line": "Idle · export after sim pass",
        "agent_context": "",
        "inspector": "additive",
        "hud_extra": "Export stub: 3MF + G-code (require_sim_pass)",
        "show_timeline": "0",
        "show_selection": "0",
    },
    {
        "id": "sim_scientific",
        "chip": "Scientific",
        "journey": "profile_particle_tier_md",
        "viewport_label": "MD viewport · NVE integrator",
        "agent_line": "Running · energy drift check",
        "agent_context": "",
        "inspector": "sci",
        "hud_extra": "Particles: 10,000 (display tier) · MD label",
        "show_timeline": "0",
        "show_selection": "0",
    },
    {
        "id": "sim_drug_design",
        "chip": "Drug design",
        "journey": "profile_adaptive_stage_hint",
        "viewport_label": "Binding site A · adaptive layout",
        "agent_line": "Running · Optimize binding pose",
        "agent_context": "",
        "inspector": "drug",
        "hud_extra": "Adaptive stage: pose refinement (studio.adaptive stub)",
        "show_timeline": "0",
        "show_selection": "1",
    },
]

INSPECTOR_BLOCKS: dict[str, str] = {
    "selection": """
      <h2>Inspector</h2>
      <p>Selection: <strong>Prop_01</strong></p>
      <dl class="kv">
        <dt>Transform</dt><dd>1, 0, 0 · 0°</dd>
        <dt>Layer</dt><dd>default</dd>
      </dl>
    """,
    "rl": """
      <h2>Inspector</h2>
      <p>Env: <strong>CartPole-v1</strong></p>
      <dl class="kv">
        <dt>Obs dim</dt><dd>4</dd>
        <dt>Action</dt><dd>discrete(2)</dd>
        <dt>Step</dt><dd>12,420</dd>
      </dl>
    """,
    "auto": """
      <h2>Inspector</h2>
      <p>Vehicle: <strong>none</strong></p>
      <div class="empty-hint">Load a map + vehicle profile to populate sensors.</div>
    """,
    "robotics": """
      <h2>Inspector</h2>
      <p>Selection: <strong>Link_3</strong></p>
      <dl class="kv">
        <dt>Joint</dt><dd>revolute</dd>
        <dt>θ</dt><dd>1.57 rad</dd>
        <dt>τ limit</dt><dd>120 N·m</dd>
        <dt>IK</dt><dd>stub</dd>
      </dl>
    """,
    "additive": """
      <h2>Inspector</h2>
      <p>Job: <strong>bench_bracket</strong></p>
      <dl class="kv">
        <dt>Material</dt><dd>PLA</dd>
        <dt>Layers</dt><dd>240</dd>
      </dl>
      <div class="export-hint">Export (stub): <strong>3MF</strong> · <strong>G-code</strong> — enable after <code>require_sim_pass</code></div>
    """,
    "sci": """
      <h2>Inspector</h2>
      <p>System: <strong>LJ fluid</strong></p>
      <dl class="kv">
        <dt>Integrator</dt><dd>NVE</dd>
        <dt>Particles</dt><dd>10,000</dd>
        <dt>Tier</dt><dd>display</dd>
      </dl>
    """,
    "drug": """
      <h2>Inspector</h2>
      <p>Selection: <strong>Molecule A</strong></p>
      <dl class="kv">
        <dt>Atoms</dt><dd>48</dd>
        <dt>Force field</dt><dd>AMBER ff14SB</dd>
        <dt>Stage</dt><dd>adaptive · pose refine</dd>
      </dl>
      <div class="empty-hint">Lab-in-the-Loop adaptive GUI hint (not live).</div>
    """,
}


def render(p: dict[str, str]) -> str:
    vp_label = p["viewport_label"]
    vp_overlay = ""
    if vp_label:
        vp_overlay = f'<div class="viewport-placeholder">{vp_label}</div>'
    selection = (
        '<div class="selection-ring" aria-hidden="true"></div>'
        if p["show_selection"] == "1"
        else ""
    )
    timeline = ""
    if p["show_timeline"] == "1":
        timeline = """
    <section class="timeline" aria-label="Timeline">
      <div style="display:flex;justify-content:space-between;align-items:center;">
        <strong>Timeline</strong>
        <span style="font-size:12px;color:var(--color-text-muted);">Frame 142 / 480 · 24 fps · playing</span>
      </div>
      <div class="timeline-track"><div class="playhead"></div></div>
    </section>
"""
    agent_ctx = ""
    if p["agent_context"]:
        agent_ctx = f'<span class="agent-context">{p["agent_context"]}</span>'
    inspector = INSPECTOR_BLOCKS[p["inspector"]]
    bottom = "180px" if p["show_timeline"] == "1" else "0"
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=1920, height=1080" />
  <title>Li World Studio — {p['id']} (marketing mock)</title>
  <link rel="stylesheet" href="../studio-tokens.css" />
  <style>
    * {{ box-sizing: border-box; }}
    body {{
      width: 1920px; height: 1080px; margin: 0;
      background: var(--color-bg-primary);
      color: var(--color-text-primary);
      font-family: var(--typography-ui);
      overflow: hidden;
    }}
    .mock-banner {{
      position: absolute; top: 0; left: 0; right: 0; z-index: 100;
      background: var(--mock-banner-bg, #3d1f00);
      color: #ffdf9e; font-size: 11px; letter-spacing: 0.04em;
      text-align: center; padding: 4px 8px;
      border-bottom: 1px solid var(--color-accent-amber);
    }}
    .shell {{ position: absolute; inset: 24px 0 0 0; }}
    .dock {{
      position: absolute; left: 0; top: 0; bottom: 0;
      width: var(--spacing-dock-width-px);
      background: var(--color-bg-elevated);
      border-right: 1px solid var(--color-border);
      display: flex; flex-direction: column; align-items: center;
      padding: 12px 0; gap: 8px;
    }}
    .dock button {{
      width: 36px; height: 36px; border: 1px solid var(--color-border);
      border-radius: 6px; background: transparent; color: var(--color-text-muted);
    }}
    .dock button.active {{
      border-color: var(--color-accent-cyan);
      color: var(--color-accent-cyan);
    }}
    .topbar {{
      position: absolute; left: var(--spacing-dock-width-px); right: 0; top: 0;
      height: var(--spacing-topbar-height-px);
      background: var(--color-bg-elevated);
      border-bottom: 1px solid var(--color-border);
      display: flex; align-items: center; gap: 16px; padding: 0 16px;
    }}
    .profile-chip {{
      font-size: 12px; padding: 4px 10px; border-radius: 999px;
      border: 1px solid var(--color-accent-violet); color: var(--color-accent-violet);
    }}
    .viewport {{
      position: absolute;
      left: var(--spacing-dock-width-px); top: var(--spacing-topbar-height-px);
      right: var(--spacing-inspector-width-px); bottom: {bottom};
      background: radial-gradient(circle at 50% 42%, #1a2332, var(--color-bg-primary));
      border-right: 1px solid var(--color-border);
    }}
    .viewport-grid {{
      position: absolute; inset: 0;
      background-image:
        linear-gradient(var(--color-viewport-grid) 1px, transparent 1px),
        linear-gradient(90deg, var(--color-viewport-grid) 1px, transparent 1px);
      background-size: 48px 48px; opacity: 0.45;
    }}
    .viewport-placeholder {{
      position: absolute; left: 50%; top: 50%; transform: translate(-50%, -50%);
      max-width: 70%; text-align: center; padding: 16px 24px;
      border: 2px dashed var(--color-accent-amber); color: var(--color-accent-amber);
      font-size: 14px; background: rgba(13, 17, 23, 0.9); border-radius: 8px;
    }}
    .selection-ring {{
      position: absolute; left: 50%; top: 45%; width: 120px; height: 120px;
      transform: translate(-50%, -50%);
      border: 2px solid var(--color-accent-cyan);
      border-radius: 50%;
    }}
    .hud {{
      position: absolute; left: 16px; bottom: 16px;
      font-family: var(--typography-mono); font-size: 11px;
      color: var(--color-accent-mint);
      background: rgba(13, 17, 23, 0.85); padding: 8px 12px;
      border: 1px solid var(--color-border); border-radius: 4px;
    }}
    .hud .mock-metric {{ color: var(--color-text-muted); font-size: 10px; }}
    .timeline {{
      position: absolute;
      left: var(--spacing-dock-width-px); right: var(--spacing-inspector-width-px); bottom: 0;
      height: 180px; background: var(--color-bg-elevated);
      border-top: 1px solid var(--color-border); padding: 12px 16px;
    }}
    .timeline-track {{
      height: 48px; margin-top: 8px; border-radius: 4px;
      background: linear-gradient(90deg, var(--color-border) 1px, transparent 1px);
      background-size: 40px 100%; position: relative;
    }}
    .playhead {{
      position: absolute; left: 35%; top: 0; bottom: 0; width: 2px;
      background: var(--color-accent-amber);
    }}
    .inspector {{
      position: absolute; right: 0; top: var(--spacing-topbar-height-px);
      width: var(--spacing-inspector-width-px); bottom: 0;
      background: var(--color-bg-elevated);
      border-left: 1px solid var(--color-border); padding: 16px;
      font-size: 13px;
    }}
    .inspector h2 {{ margin: 0 0 8px; font-size: 14px; }}
    .kv {{ display: grid; grid-template-columns: 100px 1fr; gap: 6px 12px; margin-top: 12px; }}
    .kv dt {{ color: var(--color-text-muted); }}
    .export-hint {{
      margin-top: 12px; padding: 10px; border-radius: 6px;
      border: 1px solid var(--color-accent-mint); color: var(--color-accent-mint);
      font-size: 12px;
    }}
    .empty-hint {{
      margin-top: 16px; padding: 12px; border-radius: 6px;
      border: 1px dashed var(--color-border); color: var(--color-text-muted);
      font-size: 12px;
    }}
    .agent-strip {{
      position: absolute; right: var(--spacing-inspector-width-px); left: var(--spacing-dock-width-px);
      bottom: {bottom}; height: 40px;
      display: flex; align-items: center; gap: 12px; padding: 0 16px;
      background: rgba(22, 27, 34, 0.95);
      border-top: 1px solid var(--color-border);
      font-size: 12px;
    }}
    .agent-status {{
      padding: 4px 10px; border-radius: 4px;
      background: rgba(35, 134, 54, 0.2); color: var(--color-agent-running);
      border: 1px solid var(--color-agent-running);
    }}
    .agent-context {{ color: var(--color-accent-cyan); font-size: 11px; }}
  </style>
</head>
<body>
  <div class="mock-banner" role="note">MARKETING MOCK — profile: {p['id']} · native_product: false · compose/paint is native; pixels here are HTML</div>
  <div class="shell">
    <nav class="dock" aria-label="Tools">
      <button type="button" class="active" title="Select">◇</button>
      <button type="button" title="Move">✥</button>
    </nav>
    <header class="topbar">
      <strong>Li World Studio</strong>
      <span class="profile-chip">{p['chip']}</span>
    </header>
    <main class="viewport" aria-label="3D viewport">
      <div class="viewport-grid" aria-hidden="true"></div>
      {vp_overlay}
      {selection}
      <div class="hud">
        <div>{p['hud_extra']}</div>
        <div class="mock-metric">lic check · studio_compose_shell_profile · no wgpu pixels in this capture</div>
      </div>
    </main>
    <aside class="agent-strip" aria-label="Agent copilot">
      <span class="agent-status">{p['agent_line']}</span>
      {agent_ctx}
    </aside>
{timeline}
    <aside class="inspector" aria-label="Inspector">
{inspector}
    </aside>
  </div>
</body>
</html>
"""


def main() -> None:
    mocks = []
    for p in PROFILES:
        out = ROOT / f"{p['id']}.html"
        out.write_text(render(p), encoding="utf-8")
        mocks.append(
            {
                "file": out.name,
                "profile": p["id"],
                "journey": p["journey"],
                "native_product": False,
            }
        )
    manifest = {
        "kind": "studio-vertical-mocks",
        "native_product": False,
        "capture_script": "capture.sh",
        "profiles": [x["id"] for x in PROFILES],
        "mocks": mocks,
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(mocks)} mocks under {ROOT}")


if __name__ == "__main__":
    main()
