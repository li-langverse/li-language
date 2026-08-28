#!/usr/bin/env python3
"""Apply UX-07 empty states to packages/li-studio/src/lib.li (idempotent)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "packages/li-studio/src/lib.li"


def main() -> int:
    text = LIB.read_text()
    if "viewport_empty" in text and "studio_paint_inspector_empty_cmds" in text:
        print("already applied")
        return 0

    text = text.replace(
        "# li-studio — compose dock + timeline + inspector + agent chrome (PH-UX-05/06).",
        "# li-studio — compose dock + timeline + inspector + agent chrome (PH-UX-05/06/07).",
    )

    anchor = """def studio_inspector_field_rows() -> int
  requires true
  ensures result == 4
  decreases 0
=
  return 4

type StudioDockCompose = object"""
    if anchor not in text:
        sys.exit("missing field_rows anchor")
    extra = anchor.replace(
        "type StudioDockCompose",
        Path(__file__).with_name("_ux07_constants.li").read_text()
        if Path(__file__).with_name("_ux07_constants.li").exists()
        else "",
    )
    # inline constants
    extra = anchor.replace(
        "type StudioDockCompose",
        """def studio_empty_hint_line_height_px() -> float
  requires true
  ensures result == 10.0
  decreases 0
=
  return 10.0

def studio_empty_hint_line_gap_px() -> float
  requires true
  ensures result == 8.0
  decreases 0
=
  return 8.0

def studio_empty_hint_box_height_px() -> float
  requires true
  ensures result == 72.0
  decreases 0
=
  return 72.0

def studio_viewport_empty_title_height_px() -> float
  requires true
  ensures result == 24.0
  decreases 0
=
  return 24.0

def studio_viewport_empty_cta_height_px() -> float
  requires true
  ensures result == 32.0
  decreases 0
=
  return 32.0

def studio_viewport_empty_cta_width_px() -> float
  requires true
  ensures result == 140.0
  decreases 0
=
  return 140.0

type StudioDockCompose""",
    )
    text = text.replace(anchor, extra)

    text = text.replace(
        """type StudioInspectorCompose = object
  public rect: Rect
  public header_rect: Rect
  public has_selection: int
  public field_rows: int

type StudioAgentProgress = object""",
        """type StudioInspectorCompose = object
  public rect: Rect
  public header_rect: Rect
  public has_selection: int
  public field_rows: int
  public empty_visible: int
  public hint_line1_rect: Rect
  public hint_line2_rect: Rect
  public hint_box_rect: Rect
  public field_row_rect: Rect

type StudioViewportEmptyCompose = object
  public frame_rect: Rect
  public empty_visible: int
  public title_rect: Rect
  public cta_rect: Rect

type StudioAgentProgress = object""",
    )

    helpers = (ROOT / "scripts/_ux07_helpers.li").read_text()
    text = text.replace(
        """type StudioShellCompose = object
  public layout: StudioShellLayout
  public dock: StudioDockCompose
  public timeline: StudioTimelineCompose
  public inspector: StudioInspectorCompose
  public agent: StudioAgentChromeCompose
  public palette: StudioCommandPaletteCompose
  public panel: GuiPanelState

def studio_dock_slot_offset_y(slot: int) -> float""",
        helpers,
    )

    m = re.search(
        r"def studio_compose_shell\(w: float, h: float, active_dock_slot: int, has_selection: int\) -> StudioShellCompose\n"
        r"[\s\S]*?\n  return out\n",
        text,
    )
    if not m:
        sys.exit("compose_shell not found")
    new_shell = (ROOT / "scripts/_ux07_compose_shell.li").read_text()
    text = text[: m.start()] + new_shell + text[m.end() :]

    text = text.replace(
        "studio_compose_shell(w0, h0, active_dock_slot, has_selection)",
        "studio_compose_shell(w0, h0, active_dock_slot, has_selection, 0)",
    )

    text = re.sub(
        r"def studio_compose_shell_agent\(w: float, h: float, active_dock_slot: int, has_selection: int, task_state: int\)",
        "def studio_compose_shell_agent(w: float, h: float, active_dock_slot: int, has_selection: int, scene_entity_count: int, task_state: int)",
        text,
        count=1,
    )
    text = text.replace(
        "  requires has_selection <= 1\n  requires task_state >= studio_agent_task_idle()",
        "  requires has_selection <= 1\n  requires scene_entity_count >= 0\n  requires task_state >= studio_agent_task_idle()",
        1,
    )
    text = text.replace(
        "  var sel0: int = has_selection\n  var task0: int = task_state",
        "  var sel0: int = has_selection\n  var scene0: int = scene_entity_count\n  var task0: int = task_state",
        1,
    )
    text = text.replace(
        "studio_compose_shell(w0, h0, active_dock_slot, has_selection)",
        "studio_compose_shell(w0, h0, active_dock_slot, has_selection, scene0)",
        1,
    )

    paint_cmds = (ROOT / "scripts/_ux07_paint_cmds.li").read_text()
    text = text.replace(
        """def studio_paint_inspector_cmds(has_selection: int) -> int
  requires has_selection >= 0
  requires has_selection <= 1
  ensures result >= 2
  ensures result <= 3
  decreases has_selection
=
  if has_selection == 0:
    return 2
  return 3""",
        paint_cmds,
    )

    text = text.replace(
        """def studio_paint_inspector(frame: var PaintFrame, inspector: StudioInspectorCompose) -> unit
  requires frame.cmd_count >= 0
  ensures frame.cmd_count == old(frame.cmd_count) + studio_paint_inspector_cmds(inspector.has_selection)
  decreases inspector.has_selection
=
  frame.cmd_count = frame.cmd_count + studio_paint_inspector_cmds(inspector.has_selection)
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = inspector.rect
  frame.last_color = studio_color_accent_violet()""",
        (ROOT / "scripts/_ux07_paint_inspector.li").read_text(),
    )

    # agent paint cmd arity
    agent_cmd = "studio_paint_agent_cmds(task_state)"
    if agent_cmd in text:
        text = text.replace(agent_cmd, "studio_paint_agent_cmds(task_state, studio_agent_context_none())")

    chrome = (ROOT / "scripts/_ux07_chrome.li").read_text()
    text = text.replace(
        """def studio_shell_chrome_count(has_selection: int, task_state: int) -> int
  requires has_selection >= 0
  requires has_selection <= 1
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result >= studio_paint_compose_panels_count(has_selection)
  decreases task_state
=
  return studio_shell_chrome_count_palette(has_selection, task_state, studio_palette_closed_flag())

def studio_shell_chrome_count_palette(has_selection: int, task_state: int, palette_open: int) -> int
  requires has_selection >= 0
  requires has_selection <= 1
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  requires palette_open >= studio_palette_closed_flag()
  requires palette_open <= studio_palette_open_flag()
  ensures result >= studio_paint_compose_panels_count(has_selection)
  decreases task_state
=
  return studio_paint_compose_panels_count(has_selection) + 2 + studio_paint_agent_cmds(task_state, studio_agent_context_none()) + studio_paint_palette_cmds(palette_open)""",
        chrome,
    )

    text = text.replace(
        """def studio_paint_shell_chrome(frame: var PaintFrame, compose: StudioShellCompose) -> unit
  requires frame.cmd_count >= 0
  ensures frame.cmd_count == studio_shell_chrome_count_palette(compose.inspector.has_selection, compose.agent.task_state, compose.palette.is_open)
  ensures frame.last_kind == paint_op_viewport_grid()
  ensures frame.last_rect.w == compose.layout.viewport.w
  decreases compose.agent.task_state
=
  frame.cmd_count = frame.cmd_count + studio_paint_dock_cmds()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = compose.dock.active_slot_rect
  frame.last_color = studio_color_accent_cyan()
  frame.cmd_count = frame.cmd_count + studio_paint_timeline_cmds()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = compose.timeline.playhead_rect
  frame.last_color = studio_color_accent_amber()
  frame.cmd_count = frame.cmd_count + studio_paint_inspector_cmds(compose.inspector.has_selection)
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = compose.inspector.rect
  frame.last_color = studio_color_accent_violet()
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = compose.layout.topbar
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_viewport_grid()
  frame.last_rect = compose.layout.viewport
  frame.last_color = studio_color_accent_cyan()
  studio_paint_agent(frame, compose.agent)
  paint_studio_palette(frame, compose.palette)""",
        (ROOT / "scripts/_ux07_paint_shell.li").read_text(),
    )

    text = text.replace(
        "studio_shell_chrome_count(1, studio_agent_task_running())",
        "studio_shell_chrome_count(1, 1, studio_agent_task_running())",
    )
    text = text.replace(
        "studio_compose_shell_agent(w, h, 0, 1, studio_agent_task_running())",
        "studio_compose_shell_agent(w, h, 0, 1, 1, studio_agent_task_running())",
    )

    LIB.write_text(text)
    print(f"wrote {LIB} ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
