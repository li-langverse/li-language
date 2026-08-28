#!/usr/bin/env python3
"""Apply UX-07 + integration fixes atomically to packages/li-studio/src/lib.li."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "packages/li-studio/src/lib.li"


def main() -> int:
    text = LIB.read_text()
    if "viewport_empty" not in text or "studio_paint_inspector_empty_cmds" not in text:
        subprocess.run([sys.executable, str(ROOT / "scripts/apply-ux07-li-studio.py")], check=True)
        text = LIB.read_text()

    if "def studio_compose_shell_palette(" not in text:
        palette_fn = '''
def studio_compose_shell_palette(w: float, h: float, active_dock_slot: int, has_selection: int, palette_open: int) -> StudioShellCompose
  requires w > 0.0
  requires h > 0.0
  requires active_dock_slot >= 0
  requires active_dock_slot < studio_dock_slot_count()
  requires has_selection >= 0
  requires has_selection <= 1
  requires palette_open >= studio_palette_closed_flag()
  requires palette_open <= studio_palette_open_flag()
  ensures result.palette.is_open == palette_open
  decreases palette_open
=
  var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, has_selection, 0)
  out.palette = studio_compose_palette(out.layout.viewport_w, out.layout.viewport_h, palette_open)
  return out

'''
        text = text.replace(
            "\ndef studio_compose_shell_agent(",
            palette_fn + "\ndef studio_compose_shell_agent(",
            1,
        )

    if "out.config = studio_project_config_new(studio_profile_game())" not in text:
        text = text.replace(
            "  out.agent = studio_compose_agent_chrome(out.layout, studio_agent_task_idle(), studio_agent_context_none())\n  out.palette = studio_compose_palette(out.layout.viewport_w, out.layout.viewport_h, studio_palette_closed_flag())",
            "  out.config = studio_project_config_new(studio_profile_game())\n  out.agent = studio_compose_agent_chrome(out.layout, studio_agent_task_idle(), studio_agent_context_none())\n  out.palette = studio_compose_palette(out.layout.viewport_w, out.layout.viewport_h, studio_palette_closed_flag())",
            1,
        )

    old_paint_shell = re.search(
        r"def studio_paint_shell_chrome\(frame: var PaintFrame, compose: StudioShellCompose\) -> unit\n"
        r"[\s\S]*?\n  paint_studio_palette\(frame, compose\.palette\)\n",
        text,
    )
    if old_paint_shell:
        new_paint_shell = (ROOT / "scripts/_ux07_paint_shell.li").read_text()
        new_paint_shell = new_paint_shell.replace(
            "  ensures frame.cmd_count == studio_shell_chrome_count_palette(compose.inspector.has_selection, compose.scene_entity_count, compose.agent.task_state, compose.palette.is_open)\n",
            "  ensures frame.cmd_count == studio_shell_chrome_count_palette(compose.inspector.has_selection, compose.scene_entity_count, compose.agent.task_state, compose.palette.is_open, compose.agent.agent_context_label)\n",
        )
        new_paint_shell = new_paint_shell.replace(
            "  frame.cmd_count = frame.cmd_count + 1\n  frame.last_kind = paint_op_fill_rect()\n  frame.last_rect = compose.layout.topbar\n  frame.last_color = studio_color_bg_elevated()\n",
            "  frame.cmd_count = frame.cmd_count + 1\n  frame.last_kind = paint_op_fill_rect()\n  frame.last_rect = compose.layout.topbar\n  frame.last_color = studio_color_bg_elevated()\n  studio_paint_topbar_profile(frame, compose.layout.topbar, studio_active_profile(compose))\n",
        )
        text = text[: old_paint_shell.start()] + new_paint_shell + text[old_paint_shell.end() :]

    text = text.replace(
        "  return studio_paint_compose_panels_count(has_selection) + 1 + studio_shell_viewport_cmds(scene_entity_count) + studio_paint_agent_cmds(task_state, agent_context_label) + studio_paint_palette_cmds(palette_open)",
        "  return studio_paint_compose_panels_count(has_selection) + 1 + studio_paint_topbar_profile_cmds() + studio_shell_viewport_cmds(scene_entity_count) + studio_paint_agent_cmds(task_state, agent_context_label) + studio_paint_palette_cmds(palette_open)",
    )

    text = text.replace(
        "# li-studio — compose dock + timeline + inspector + agent chrome (PH-UX-05/06).\n",
        "# li-studio — compose dock + timeline + inspector + agent chrome (PH-UX-05/06/07).\n",
    )

    text = re.sub(
        r"def li_std_studio_version\(\) -> int\n  requires true\n  ensures result == \d+\n  decreases 0\n=\n  return \d+",
        "def li_std_studio_version() -> int\n  requires true\n  ensures result == 4\n  decreases 0\n=\n  return 4",
        text,
        count=1,
    )

    tmp = LIB.with_suffix(".li.tmp")
    tmp.write_text(text)
    tmp.replace(LIB)
    print(f"wrote {LIB} ({len(text.splitlines())} lines)")
    assert "viewport_empty" in text
    assert "studio_compose_shell_palette" in text
    assert "studio_paint_viewport_empty" in text
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
