#!/usr/bin/env python3
"""Apply UX-08 viewport error recovery to packages/li-studio/src/lib.li (HEAD baseline)."""
from __future__ import annotations

from pathlib import Path

LIB = Path(__file__).resolve().parents[1] / "packages/li-studio/src/lib.li"


def main() -> None:
    text = LIB.read_text()
    if "studio_err_gpu" in text:
        print("already applied")
        return

    text = text.replace(
        "extern proc li_rt_studio_parse_toml_profile_line(line: str) -> int\n"
        "  requires true\n  ensures result >= 0\n  decreases 0\n\n",
        "extern proc li_rt_studio_parse_toml_profile_line(line: str) -> int\n"
        "  requires true\n  ensures result >= 0\n  decreases 0\n\n"
        "# UX-08 — viewport error mock (no wgpu surface probe; native host wires later).\n"
        "extern proc li_rt_studio_viewport_error_kind() -> int\n"
        "extern proc li_rt_studio_viewport_error_set_mock(kind: int) -> int\n"
        "extern proc li_rt_studio_viewport_error_retry() -> int\n\n",
    )

    text = text.replace(
        "  return 140.0\n\ntype StudioDockCompose = object",
        """  return 140.0

def studio_viewport_error_none() -> int
  requires true
  ensures result == 0
  decreases 0
=
  return 0

def studio_err_gpu() -> int
  requires true
  ensures result == 1
  decreases 0
=
  return 1

def studio_err_missing_asset() -> int
  requires true
  ensures result == 2
  decreases 0
=
  return 2

def studio_viewport_error_message_height_px() -> float
  requires true
  ensures result == 48.0
  decreases 0
=
  return 48.0

def studio_viewport_error_retry_width_px() -> float
  requires true
  ensures result == 100.0
  decreases 0
=
  return 100.0

def studio_viewport_error_retry_height_px() -> float
  requires true
  ensures result == 32.0
  decreases 0
=
  return 32.0

def studio_viewport_error_kind() -> int
  requires true
  ensures result >= studio_viewport_error_none()
  ensures result <= studio_err_missing_asset()
  decreases 0
=
  return li_rt_studio_viewport_error_kind()

def studio_viewport_error_set_mock(kind: int) -> int
  requires kind >= studio_viewport_error_none()
  requires kind <= studio_err_missing_asset()
  ensures result == kind
  decreases kind
=
  return li_rt_studio_viewport_error_set_mock(kind)

def studio_viewport_error_retry() -> int
  requires true
  ensures result == studio_viewport_error_none()
  ensures studio_viewport_error_kind() == studio_viewport_error_none()
  decreases 0
=
  return li_rt_studio_viewport_error_retry()

def studio_viewport_error_visible(error_kind: int) -> int
  requires error_kind >= studio_viewport_error_none()
  requires error_kind <= studio_err_missing_asset()
  ensures result >= 0
  ensures result <= 1
  decreases error_kind
=
  if error_kind == studio_viewport_error_none():
    return 0
  return 1

def studio_viewport_error_message_rect_at(vp_x: float, vp_y: float, vp_w: float, vp_h: float) -> Rect
  requires vp_w >= studio_viewport_error_retry_width_px()
  requires vp_h >= studio_viewport_error_message_height_px() + studio_viewport_error_retry_height_px() + 32.0
  ensures result.w >= studio_viewport_error_retry_width_px()
  ensures result.h == studio_viewport_error_message_height_px()
  decreases 0
=
  var msg_w: float = studio_viewport_error_retry_width_px() + 120.0
  if msg_w > vp_w - 32.0:
    msg_w = vp_w - 32.0
  var x: float = vp_x + (vp_w - msg_w) / 2.0
  var y: float = vp_y + (vp_h * 0.40)
  return rect_make(x, y, msg_w, studio_viewport_error_message_height_px())

def studio_viewport_error_retry_rect_at(vp_x: float, vp_y: float, vp_w: float, vp_h: float) -> Rect
  requires vp_w >= studio_viewport_error_retry_width_px()
  requires vp_h >= studio_viewport_error_retry_height_px() + 32.0
  ensures result.w == studio_viewport_error_retry_width_px()
  ensures result.h == studio_viewport_error_retry_height_px()
  decreases 0
=
  var x: float = vp_x + (vp_w - studio_viewport_error_retry_width_px()) / 2.0
  var y: float = vp_y + (vp_h * 0.52)
  return rect_make(x, y, studio_viewport_error_retry_width_px(), studio_viewport_error_retry_height_px())

type StudioDockCompose = object""",
    )

    text = text.replace(
        "type StudioViewportEmptyCompose = object\n"
        "  public frame_rect: Rect\n"
        "  public empty_visible: int\n"
        "  public title_rect: Rect\n"
        "  public cta_rect: Rect\n\n",
        "type StudioViewportEmptyCompose = object\n"
        "  public frame_rect: Rect\n"
        "  public empty_visible: int\n"
        "  public title_rect: Rect\n"
        "  public cta_rect: Rect\n\n"
        "type StudioViewportErrorOverlay = object\n"
        "  public frame_rect: Rect\n"
        "  public error_kind: int\n"
        "  public visible: int\n"
        "  public message_rect: Rect\n"
        "  public retry_rect: Rect\n\n",
    )

    text = text.replace(
        "  public viewport_empty: StudioViewportEmptyCompose\n  public agent: StudioAgentChromeCompose",
        "  public viewport_empty: StudioViewportEmptyCompose\n  public viewport_error: StudioViewportErrorOverlay\n  public agent: StudioAgentChromeCompose",
    )

    idx = text.find("def studio_compose_empty_viewport")
    end = text.find("\ndef studio_dock_slot_offset_y", idx)
    text = (
        text[:end]
        + """

def studio_compose_viewport_error_overlay(layout: StudioShellLayout, error_kind: int) -> StudioViewportErrorOverlay
  requires layout.viewport.w > 0.0
  requires layout.viewport.h > 0.0
  requires error_kind >= studio_viewport_error_none()
  requires error_kind <= studio_err_missing_asset()
  ensures result.error_kind == error_kind
  ensures result.visible == studio_viewport_error_visible(error_kind)
  decreases error_kind
=
  var out: StudioViewportErrorOverlay
  out.frame_rect = layout.viewport
  out.error_kind = error_kind
  out.visible = studio_viewport_error_visible(error_kind)
  out.message_rect = rect_make(0.0, 0.0, 0.0, 0.0)
  out.retry_rect = rect_make(0.0, 0.0, 0.0, 0.0)
  if out.visible == 1:
    out.message_rect = studio_viewport_error_message_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)
    out.retry_rect = studio_viewport_error_retry_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)
  return out

def studio_viewport_error_ok(overlay: StudioViewportErrorOverlay) -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0
=
  if overlay.visible == 0:
    if overlay.error_kind != studio_viewport_error_none():
      return 0
    return 1
  if overlay.error_kind == studio_viewport_error_none():
    return 0
  if overlay.message_rect.h != studio_viewport_error_message_height_px():
    return 0
  if overlay.retry_rect.w != studio_viewport_error_retry_width_px():
    return 0
  return 1
"""
        + text[end:]
    )

    text = text.replace(
        "  out.viewport_empty.cta_rect = studio_viewport_empty_cta_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)\n  out.agent = studio_compose_agent_chrome(layout, studio_agent_task_idle(), studio_agent_context_none())",
        "  out.viewport_empty.cta_rect = studio_viewport_empty_cta_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)\n  out.viewport_error = studio_compose_viewport_error_overlay(layout, studio_viewport_error_kind())\n  out.agent = studio_compose_agent_chrome(layout, studio_agent_task_idle(), studio_agent_context_none())",
        1,
    )

    text = text.replace(
        "    out.viewport_empty.cta_rect = rect_make(0.0, 0.0, 0.0, 0.0)\n  out.agent = studio_compose_agent_chrome(out.layout, studio_agent_task_idle(), studio_agent_context_none())",
        "    out.viewport_empty.cta_rect = rect_make(0.0, 0.0, 0.0, 0.0)\n  out.viewport_error = studio_compose_viewport_error_overlay(out.layout, studio_viewport_error_kind())\n  out.agent = studio_compose_agent_chrome(out.layout, studio_agent_task_idle(), studio_agent_context_none())",
        1,
    )

    text = text.replace(
        "  var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, has_selection)\n  out.agent = studio_compose_agent_chrome(out.layout, task_state, studio_agent_context_for_shell(has_selection, task_state))",
        "  var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, has_selection, scene_entity_count)\n  out.agent = studio_compose_agent_chrome(out.layout, task_state, studio_agent_context_for_shell(has_selection, task_state))",
    )

    text = text.replace(
        "def studio_shell_chrome_count(has_selection: int, task_state: int) -> int\n"
        "  requires has_selection >= 0\n"
        "  requires has_selection <= 1\n"
        "  requires task_state >= studio_agent_task_idle()\n"
        "  requires task_state <= studio_agent_task_done()\n"
        "  ensures result >= studio_paint_compose_panels_count(has_selection)\n"
        "  decreases task_state\n"
        "=\n"
        "  return studio_shell_chrome_count_palette(has_selection, task_state, studio_palette_closed_flag(), studio_agent_context_for_shell(has_selection, task_state))\n\n"
        "def studio_shell_chrome_count_palette(has_selection: int, task_state: int, palette_open: int, agent_context_label: int) -> int\n"
        "  requires has_selection >= 0\n"
        "  requires has_selection <= 1\n"
        "  requires task_state >= studio_agent_task_idle()\n"
        "  requires task_state <= studio_agent_task_done()\n"
        "  requires palette_open >= studio_palette_closed_flag()\n"
        "  requires palette_open <= studio_palette_open_flag()\n"
        "  requires agent_context_label >= studio_agent_context_none()\n"
        "  requires agent_context_label <= studio_agent_context_selection()\n"
        "  ensures result >= studio_paint_compose_panels_count(has_selection)\n"
        "  decreases task_state\n"
        "=\n"
        "  return studio_paint_compose_panels_count(has_selection) + 2 + studio_paint_agent_cmds(task_state, agent_context_label) + studio_paint_palette_cmds(palette_open)\n",
        """def studio_shell_viewport_cmds(scene_entity_count: int, viewport_error_kind: int) -> int
  requires scene_entity_count >= 0
  requires viewport_error_kind >= studio_viewport_error_none()
  requires viewport_error_kind <= studio_err_missing_asset()
  ensures result >= 1
  ensures result <= 3
  decreases viewport_error_kind
=
  if studio_viewport_error_visible(viewport_error_kind) == 1:
    return studio_paint_viewport_error_cmds()
  if scene_entity_count == 0:
    return studio_paint_viewport_empty_cmds()
  return studio_paint_viewport_scene_cmds()

def studio_shell_chrome_count(has_selection: int, scene_entity_count: int, task_state: int) -> int
  requires has_selection >= 0
  requires has_selection <= 1
  requires scene_entity_count >= 0
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result >= studio_paint_compose_panels_count(has_selection)
  decreases task_state
=
  return studio_shell_chrome_count_palette(has_selection, scene_entity_count, task_state, studio_palette_closed_flag(), studio_agent_context_for_shell(has_selection, task_state), studio_viewport_error_none())

def studio_shell_chrome_count_palette(has_selection: int, scene_entity_count: int, task_state: int, palette_open: int, agent_context_label: int, viewport_error_kind: int) -> int
  requires has_selection >= 0
  requires has_selection <= 1
  requires scene_entity_count >= 0
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  requires palette_open >= studio_palette_closed_flag()
  requires palette_open <= studio_palette_open_flag()
  requires agent_context_label >= studio_agent_context_none()
  requires agent_context_label <= studio_agent_context_selection()
  requires viewport_error_kind >= studio_viewport_error_none()
  requires viewport_error_kind <= studio_err_missing_asset()
  ensures result >= studio_paint_compose_panels_count(has_selection)
  decreases task_state
=
  return studio_paint_compose_panels_count(has_selection) + 2 + studio_shell_viewport_cmds(scene_entity_count, viewport_error_kind) + studio_paint_agent_cmds(task_state, agent_context_label) + studio_paint_palette_cmds(palette_open)
""",
    )

    text = text.replace(
        "def studio_paint_viewport_empty_cmds() -> int\n"
        "  requires true\n"
        "  ensures result == 3\n"
        "  decreases 0\n"
        "=\n"
        "  return 3\n\n"
        "def studio_paint_viewport_scene_cmds() -> int",
        """def studio_paint_viewport_error_cmds() -> int
  requires true
  ensures result == 2
  decreases 0
=
  return 2

def studio_paint_viewport_error(frame: var PaintFrame, overlay: StudioViewportErrorOverlay) -> unit
  requires frame.cmd_count >= 0
  requires overlay.visible == 1
  ensures frame.cmd_count == old(frame.cmd_count) + studio_paint_viewport_error_cmds()
  ensures frame.last_kind == paint_op_stroke_rect()
  decreases overlay.error_kind
=
  frame.cmd_count = frame.cmd_count + studio_paint_viewport_error_cmds()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = overlay.message_rect
  frame.last_color = studio_color_agent_error()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = overlay.retry_rect
  frame.last_color = studio_color_accent_cyan()

def studio_paint_viewport_empty(frame: var PaintFrame, viewport_empty: StudioViewportEmptyCompose) -> unit
  requires frame.cmd_count >= 0
  requires viewport_empty.empty_visible == 1
  ensures frame.cmd_count == old(frame.cmd_count) + studio_paint_viewport_empty_cmds()
  decreases 0
=
  frame.cmd_count = frame.cmd_count + studio_paint_viewport_empty_cmds()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = viewport_empty.frame_rect
  frame.last_color = studio_color_border()
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = viewport_empty.title_rect
  frame.last_color = studio_color_border()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = viewport_empty.cta_rect
  frame.last_color = studio_color_border()

def studio_paint_viewport_empty_cmds() -> int
  requires true
  ensures result == 3
  decreases 0
=
  return 3

def studio_paint_viewport_scene_cmds() -> int""",
    )

    text = text.replace(
        "def studio_paint_shell_chrome(frame: var PaintFrame, compose: StudioShellCompose) -> unit\n"
        "  requires frame.cmd_count >= 0\n"
        "  ensures frame.cmd_count == studio_shell_chrome_count_palette(compose.inspector.has_selection, compose.agent.task_state, compose.palette.is_open, compose.agent.agent_context_label)\n"
        "  ensures frame.last_kind == paint_op_viewport_grid()\n"
        "  ensures frame.last_rect.w == compose.layout.viewport.w\n"
        "  decreases compose.agent.task_state\n"
        "=\n"
        "  frame.cmd_count = frame.cmd_count + studio_paint_dock_cmds()\n"
        "  frame.last_kind = paint_op_stroke_rect()\n"
        "  frame.last_rect = compose.dock.active_slot_rect\n"
        "  frame.last_color = studio_color_accent_cyan()\n"
        "  frame.cmd_count = frame.cmd_count + studio_paint_timeline_cmds()\n"
        "  frame.last_kind = paint_op_stroke_rect()\n"
        "  frame.last_rect = compose.timeline.playhead_rect\n"
        "  frame.last_color = studio_color_accent_amber()\n"
        "  frame.cmd_count = frame.cmd_count + studio_paint_inspector_cmds(compose.inspector.has_selection)\n"
        "  frame.last_kind = paint_op_stroke_rect()\n"
        "  frame.last_rect = compose.inspector.rect\n"
        "  frame.last_color = studio_color_accent_violet()\n"
        "  frame.cmd_count = frame.cmd_count + 1\n"
        "  frame.last_kind = paint_op_fill_rect()\n"
        "  frame.last_rect = compose.layout.topbar\n"
        "  studio_paint_topbar_profile(frame, compose.layout.topbar, studio_active_profile(compose))\n"
        "  frame.cmd_count = frame.cmd_count + 1\n"
        "  frame.last_kind = paint_op_viewport_grid()\n"
        "  frame.last_rect = compose.layout.viewport\n"
        "  frame.last_color = studio_color_accent_cyan()\n"
        "  studio_paint_agent(frame, compose.agent)\n"
        "  paint_studio_palette(frame, compose.palette)\n",
        """def studio_paint_shell_chrome(frame: var PaintFrame, compose: StudioShellCompose) -> unit
  requires frame.cmd_count >= 0
  ensures frame.cmd_count == studio_shell_chrome_count_palette(compose.inspector.has_selection, compose.scene_entity_count, compose.agent.task_state, compose.palette.is_open, compose.agent.agent_context_label, compose.viewport_error.error_kind)
  decreases compose.agent.task_state
=
  studio_paint_compose_panels(frame, compose.dock, compose.timeline, compose.inspector)
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = compose.layout.topbar
  frame.last_color = studio_color_bg_elevated()
  studio_paint_topbar_profile(frame, compose.layout.topbar, studio_active_profile(compose))
  if compose.viewport_error.visible == 1:
    studio_paint_viewport_error(frame, compose.viewport_error)
  if compose.viewport_error.visible == 0:
    if compose.scene_entity_count == 0:
      studio_paint_viewport_empty(frame, compose.viewport_empty)
    if compose.scene_entity_count > 0:
      frame.cmd_count = frame.cmd_count + studio_paint_viewport_scene_cmds()
      frame.last_kind = paint_op_viewport_grid()
      frame.last_rect = compose.layout.viewport
      frame.last_color = studio_color_accent_cyan()
  studio_paint_agent(frame, compose.agent)
  paint_studio_palette(frame, compose.palette)
""",
    )

    text = text.replace(
        "  ensures result.cmd_count == studio_shell_chrome_count(1, 1, studio_agent_task_running())",
        "  ensures result.cmd_count == studio_shell_chrome_count(1, 1, studio_agent_task_running())",
    )

    # studio_agent_chrome smoke uses studio_shell_chrome_count(0, 0, failed) - fix if present
    text = text.replace(
        "if frame_fail.cmd_count != studio_shell_chrome_count(0, 0, studio_agent_task_failed()):",
        "if frame_fail.cmd_count != studio_shell_chrome_count(0, 0, studio_agent_task_failed()):",
    )

    text = text.replace("ensures result == 4\n  decreases 0\n=\n  return 4", "ensures result == 5\n  decreases 0\n=\n  return 5")

    if "studio_err_gpu" not in text:
        raise SystemExit("patch failed")
    LIB.write_text(text)
    print("OK", LIB)


if __name__ == "__main__":
    main()
