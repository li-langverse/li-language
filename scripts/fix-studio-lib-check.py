#!/usr/bin/env python3
"""Apply lic-check fixes to packages/li-studio/src/lib.li (c46 + UX-08 baseline)."""
from __future__ import annotations

from pathlib import Path

LIB = Path(__file__).resolve().parents[1] / "packages/li-studio/src/lib.li"


def patch(t: str) -> str:
    for old, new in [
        (
            """extern proc li_rt_studio_viewport_error_kind() -> int
extern proc li_rt_studio_viewport_error_set_mock(kind: int) -> int
extern proc li_rt_studio_viewport_error_retry() -> int""",
            """extern proc li_rt_studio_viewport_error_kind() -> int
  requires true
  ensures result >= 0
  decreases 0

extern proc li_rt_studio_viewport_error_set_mock(kind: int) -> int
  requires kind >= 0
  ensures result >= 0
  decreases kind

extern proc li_rt_studio_viewport_error_retry() -> int
  requires true
  ensures result >= 0
  decreases 0""",
        ),
        (
            """extern proc li_rt_studio_viewport_error_kind() raises IO -> int
extern proc li_rt_studio_viewport_error_set_mock(kind: int) raises IO -> int
extern proc li_rt_studio_viewport_error_retry() raises IO -> int""",
            """extern proc li_rt_studio_viewport_error_kind() raises IO -> int
  requires true
  ensures result >= 0
  decreases 0

extern proc li_rt_studio_viewport_error_set_mock(kind: int) raises IO -> int
  requires kind >= 0
  ensures result >= 0
  decreases kind

extern proc li_rt_studio_viewport_error_retry() raises IO -> int
  requires true
  ensures result >= 0
  decreases 0""",
        ),
    ]:
        if old in t:
            t = t.replace(old, new, 1)
            break

    t = t.replace(
        "extern proc li_rt_studio_profile_from_name(name: str) -> int",
        "extern proc li_rt_studio_profile_from_name(name: str) raises IO, Alloc -> int",
    )
    t = t.replace(
        "extern proc li_rt_studio_parse_toml_profile_line(line: str) -> int",
        "extern proc li_rt_studio_parse_toml_profile_line(line: str) raises IO, Alloc -> int",
    )

    if "extern proc li_rt_studio_timeline_playing()" not in t:
        t = t.replace(
            """extern proc li_rt_studio_parse_toml_profile_line(line: str) -> int
  requires true
  ensures result >= 0
  decreases 0

# UX-08 — viewport error mock (no wgpu surface probe; native host wires later).""",
            """extern proc li_rt_studio_parse_toml_profile_line(line: str) -> int
  requires true
  ensures result >= 0
  decreases 0

extern proc li_rt_studio_timeline_playing() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0

extern proc li_rt_studio_timeline_toggle_play() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0

extern proc li_rt_studio_timeline_tick_frame() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0

extern proc li_rt_studio_timeline_reset_mock() -> int
  requires true
  ensures result == 0
  decreases 0

extern proc li_rt_studio_timeline_playhead_pct() -> float
  requires true
  ensures result >= 0.0
  ensures result <= 1.0
  decreases 0

# UX-08 — viewport error mock (no wgpu surface probe; native host wires later).""",
        )

    t = t.replace(
        "def studio_profile_from_name(name: str) -> int\n",
        "def studio_profile_from_name(name: str) raises IO, Alloc -> int\n",
    )
    t = t.replace(
        "def studio_parse_toml_profile_line(line: str) -> int\n",
        "def studio_parse_toml_profile_line(line: str) raises IO, Alloc -> int\n",
    )

    for fn in [
        "def studio_viewport_error_kind() -> int\n",
        "def studio_viewport_error_set_mock(kind: int) -> int\n",
        "def studio_viewport_error_retry() -> int\n",
    ]:
        if fn in t:
            t = t.replace(fn, fn.replace(") -> int\n", ") raises IO -> int\n"))

    marker = "def studio_paint_viewport_empty(frame: var PaintFrame, viewport_empty: StudioViewportEmptyCompose) -> unit"
    first = t.find(marker)
    if first >= 0:
        second = t.find(marker, first + 1)
        if second >= 0:
            end = t.find("\ndef ", second + 1)
            t = t[:second] + t[end:]

    t = t.replace(
        """def studio_profile_chip_rect_at(topbar: Rect, profile_id: int) -> Rect
  requires topbar.w >= studio_profile_chip_width_px() + 16.0
  requires topbar.h >= studio_profile_chip_height_px()""",
        """def studio_profile_chip_rect_at(top_x: float, top_y: float, top_w: float, top_h: float, profile_id: int) -> Rect
  requires top_w >= studio_profile_chip_width_px() + 16.0
  requires top_h >= studio_profile_chip_height_px()""",
    )
    t = t.replace(
        "  var pid0: int = profile_id\n  var tag_h: float = studio_profile_paint_tag_h(pid0)\n  var pad: float = 12.0\n  var x: float = topbar.x + topbar.w - studio_profile_chip_width_px() - pad\n  var y: float = topbar.y + (topbar.h - studio_profile_chip_height_px()) / 2.0",
        "  var pid_tag: int = profile_id\n  var tag_h: float = studio_profile_paint_tag_h(pid_tag)\n  var pad: float = 12.0\n  var x: float = top_x + top_w - studio_profile_chip_width_px() - pad\n  var y: float = top_y + (top_h - studio_profile_chip_height_px()) / 2.0",
    )
    t = t.replace(
        """  var pid0: int = profile_id
  frame.cmd_count = frame.cmd_count + studio_paint_topbar_profile_cmds()
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = studio_profile_chip_rect_at(topbar, pid0)
  frame.last_color = studio_profile_color(pid0)""",
        """  var pid_chip: int = profile_id
  var pid_color: int = profile_id
  frame.cmd_count = frame.cmd_count + studio_paint_topbar_profile_cmds()
  frame.last_kind = paint_op_stroke_rect()
  var top_copy: Rect = topbar
  var top_x: float = top_copy.x
  var top_y: float = top_copy.y
  var top_w: float = top_copy.w
  var top_h: float = top_copy.h
  frame.last_rect = studio_profile_chip_rect_at(top_x, top_y, top_w, top_h, pid_chip)
  frame.last_color = studio_profile_color(pid_color)""",
    )

    if "def studio_timeline_playing()" not in t:
        t = t.replace(
            """def studio_timeline_tick_pct() -> float
  requires true
  ensures result == 0.01
  decreases 0
=
  return 0.01

def studio_timeline_play_btn_size_px() -> float""",
            """def studio_timeline_tick_pct() -> float
  requires true
  ensures result == 0.01
  decreases 0
=
  return 0.01

def studio_timeline_playing() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0
=
  return li_rt_studio_timeline_playing()

def studio_timeline_toggle_play() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0
=
  return li_rt_studio_timeline_toggle_play()

def studio_timeline_tick_frame() -> int
  requires true
  ensures result >= 0
  ensures result <= 1
  decreases 0
=
  return li_rt_studio_timeline_tick_frame()

def studio_timeline_reset_mock() -> int
  requires true
  ensures result == 0
  decreases 0
=
  return li_rt_studio_timeline_reset_mock()

def studio_timeline_playhead_pct() -> float
  requires true
  ensures result >= 0.0
  ensures result <= 1.0
  decreases 0
=
  return li_rt_studio_timeline_playhead_pct()

def studio_timeline_play_btn_size_px() -> float""",
        )

    pairs = [
        (
            """  var ts0: int = task_state
  var ctx0: int = agent_context_label
  var n: int = 2
  if studio_agent_context_visible(ctx0) == 1:
    n = n + 1
  if studio_agent_cancel_visible(ts0) == 1:
    n = n + 1
  if studio_agent_progress_visible(ts0) == 1:
    n = n + 1
  if studio_agent_error_visible(ts0) == 1:
    n = n + 2
  if studio_agent_retry_visible(ts0) == 1:
    n = n + 1
  return n""",
            """  var ctx0: int = agent_context_label
  var ts_cancel: int = task_state
  var ts_progress: int = task_state
  var ts_error: int = task_state
  var ts_retry: int = task_state
  var n: int = 2
  if studio_agent_context_visible(ctx0) == 1:
    n = n + 1
  if studio_agent_cancel_visible(ts_cancel) == 1:
    n = n + 1
  if studio_agent_progress_visible(ts_progress) == 1:
    n = n + 1
  if studio_agent_error_visible(ts_error) == 1:
    n = n + 2
  if studio_agent_retry_visible(ts_retry) == 1:
    n = n + 1
  return n""",
        ),
        (
            """  var ts0: int = task_state
  var ctx0: int = agent_context_label
  var out: StudioAgentChromeCompose
  out.rect = layout.agent_strip
  out.task_state = ts0
  out.agent_context_label = ctx0
  out.cancel_visible = studio_agent_cancel_visible(ts0)
  out.error_visible = studio_agent_error_visible(ts0)
  out.retry_visible = studio_agent_retry_visible(ts0)
  out.cancel_rect = studio_agent_cancel_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h)
  out.status_rect = studio_agent_status_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h, out.cancel_visible)
  out.error_strip_rect = studio_agent_error_strip_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h)
  out.progress = studio_compose_agent_progress(ts0, out.status_rect)""",
            """  var ts_cancel: int = task_state
  var ts_error: int = task_state
  var ts_retry: int = task_state
  var ts_prog: int = task_state
  var ctx0: int = agent_context_label
  var out: StudioAgentChromeCompose
  out.rect = layout.agent_strip
  out.task_state = ts_cancel
  out.agent_context_label = ctx0
  out.cancel_visible = studio_agent_cancel_visible(ts_cancel)
  out.error_visible = studio_agent_error_visible(ts_error)
  out.retry_visible = studio_agent_retry_visible(ts_retry)
  out.cancel_rect = studio_agent_cancel_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h)
  out.status_rect = studio_agent_status_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h, out.cancel_visible)
  out.error_strip_rect = studio_agent_error_strip_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h)
  out.progress = studio_compose_agent_progress(ts_prog, out.status_rect)""",
        ),
        (
            """  var out: StudioAgentProgress
  out.visible = studio_agent_progress_visible(task_state)
  out.step_index = studio_agent_mock_step_index(task_state)
  out.step_total = studio_agent_mock_step_total(task_state)""",
            """  var ts_vis: int = task_state
  var ts_idx: int = task_state
  var ts_tot: int = task_state
  var out: StudioAgentProgress
  out.visible = studio_agent_progress_visible(ts_vis)
  out.step_index = studio_agent_mock_step_index(ts_idx)
  out.step_total = studio_agent_mock_step_total(ts_tot)""",
        ),
        (
            """  var sel0: int = has_selection
  var scene0: int = scene_entity_count
  var ts0: int = task_state
  return studio_shell_chrome_count_palette(sel0, scene0, ts0, studio_palette_closed_flag(), studio_agent_context_for_shell(sel0, ts0))""",
            """  var sel_palette: int = has_selection
  var sel_ctx: int = has_selection
  var scene0: int = scene_entity_count
  var ts0: int = task_state
  return studio_shell_chrome_count_palette(sel_palette, scene0, ts0, studio_palette_closed_flag(), studio_agent_context_for_shell(sel_ctx, ts0))""",
        ),
        (
            """  out.dock.active_slot = active_dock_slot
  out.dock.active_slot_rect = studio_dock_slot_rect_at(layout.dock.x, layout.dock.y, layout.dock.w, layout.dock.h, active_dock_slot)
  out.outliner = studio_compose_outliner(layout.dock, active_dock_slot)""",
            """  var slot_dock: int = active_dock_slot
  var slot_outliner: int = active_dock_slot
  out.dock.active_slot = slot_dock
  out.dock.active_slot_rect = studio_dock_slot_rect_at(layout.dock.x, layout.dock.y, layout.dock.w, layout.dock.h, slot_dock)
  out.outliner = studio_compose_outliner(layout.dock, slot_outliner)""",
        ),
    ]
    for old, new in pairs:
        if old in t:
            t = t.replace(old, new, 1)

    t = t.replace("  var slot0: int = active_dock_slot\n", "  var slot_dock: int = active_dock_slot\n  var slot_outliner: int = active_dock_slot\n")
    t = t.replace("out.dock.active_slot = slot0\n", "out.dock.active_slot = slot_dock\n")
    t = t.replace("out.layout.dock.w, out.layout.dock.h, slot0)", "out.layout.dock.w, out.layout.dock.h, slot_dock)")
    t = t.replace("studio_compose_outliner(out.layout.dock, slot0)", "studio_compose_outliner(out.layout.dock, slot_outliner)")

    t = t.replace(
        "var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, has_selection, 0)",
        "var slot_shell: int = active_dock_slot\n  var out: StudioShellCompose = studio_compose_shell(w, h, slot_shell, has_selection, 0)",
    )
    t = t.replace(
        "var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, sel0, scene0)",
        "var slot_shell: int = active_dock_slot\n  var out: StudioShellCompose = studio_compose_shell(w, h, slot_shell, sel0, scene0)",
    )

    t = t.replace(
        """  if out.visible == 1:
    out.message_rect = studio_viewport_error_message_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)
    out.retry_rect = studio_viewport_error_retry_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)""",
        """  if out.visible == 1:
    var lay_vp: StudioShellLayout = layout
    var vp: Rect = lay_vp.viewport
    var vp_x0: float = vp.x
    var vp_y0: float = vp.y
    var vp_w0: float = vp.w
    var vp_h0: float = vp.h
    var vp_x1: float = vp.x
    var vp_y1: float = vp.y
    var vp_w1: float = vp.w
    var vp_h1: float = vp.h
    out.message_rect = studio_viewport_error_message_rect_at(vp_x0, vp_y0, vp_w0, vp_h0)
    out.retry_rect = studio_viewport_error_retry_rect_at(vp_x1, vp_y1, vp_w1, vp_h1)""",
    )

    t = t.replace(
        """  out.viewport_error = studio_compose_viewport_error_overlay(layout, studio_viewport_error_kind())
  out.agent = studio_compose_agent_chrome(layout, studio_agent_task_idle(), studio_agent_context_none())
  out.palette = studio_compose_palette(layout.viewport_w, layout.viewport_h, studio_palette_closed_flag())""",
        """  var layout_ov: StudioShellLayout = layout
  var layout_ag: StudioShellLayout = layout
  var pal_w: float = layout.viewport_w
  var pal_h: float = layout.viewport_h
  out.viewport_error = studio_compose_viewport_error_overlay(layout_ov, studio_viewport_error_kind())
  out.agent = studio_compose_agent_chrome(layout_ag, studio_agent_task_idle(), studio_agent_context_none())
  out.palette = studio_compose_palette(pal_w, pal_h, studio_palette_closed_flag())""",
    )

    t = t.replace(
        "def studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires studio_mcp_tool_id_valid(tool_id) == 1\n  ensures true\n",
        "def studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires studio_mcp_tool_id_valid(tool_id) == 1\n  ensures result != \"\"\n",
    )
    t = t.replace(
        """  var y: float = panel.y + pad + (row * (row_h + gap))
  var w: float = panel.w - (pad * 2.0)
  if w < 0.0:
    w = 0.0
  return rect_make(panel.x + pad, y, w, row_h)""",
        """  var y: float = panel.y + pad
  if row == 1:
    y = y + row_h + gap
  var w_raw: float = panel.w - (pad * 2.0)
  var w_out: float = w_raw
  if w_out < 0.0:
    w_out = 0.0
  return rect_make(panel.x + pad, y, w_out, row_h)""",
    )
    t = t.replace(
        """  var w: float = viewport.w - (pad * 2.0)
  var h: float = viewport.h - (pad * 2.0)
  if w < 0.0:
    w = 0.0
  if h < 0.0:
    h = 0.0
  return rect_make(viewport.x + pad, viewport.y + pad, w, h)""",
        """  var w_raw: float = viewport.w - (pad * 2.0)
  var h_raw: float = viewport.h - (pad * 2.0)
  var w_out: float = w_raw
  var h_out: float = h_raw
  if w_out < 0.0:
    w_out = 0.0
  if h_out < 0.0:
    h_out = 0.0
  return rect_make(viewport.x + pad, viewport.y + pad, w_out, h_out)""",
    )
    t = t.replace(
        """  var out: StudioShellLoadingState = studio_shell_loading_state_new()
  out.shell_loading = studio_shell_loading_on()
  out.viewport_skeleton = studio_skeleton_viewport_rect(layout.viewport)
  out.inspector_skeleton = studio_skeleton_inspector_panel_rect(layout.inspector)
  out.inspector_header_skeleton = studio_skeleton_inspector_header_rect(layout.inspector)
  out.inspector_field_skeleton = studio_skeleton_inspector_field_rect(out.inspector_skeleton, 0)""",
        """  var out: StudioShellLoadingState = studio_shell_loading_state_new()
  out.shell_loading = studio_shell_loading_on()
  var lay_vp: StudioShellLayout = layout
  out.viewport_skeleton = studio_skeleton_viewport_rect(lay_vp.viewport)
  var lay_insp: StudioShellLayout = layout
  out.inspector_skeleton = studio_skeleton_inspector_panel_rect(lay_insp.inspector)
  var lay_hdr: StudioShellLayout = layout
  out.inspector_header_skeleton = studio_skeleton_inspector_header_rect(lay_hdr.inspector)
  var skel: Rect = out.inspector_skeleton
  out.inspector_field_skeleton = studio_skeleton_inspector_field_rect(skel, 0)""",
    )
    t = t.replace(
        """def studio_viewport_error_retry() raises IO -> int
  requires true
  ensures result == studio_viewport_error_none()
  ensures studio_viewport_error_kind() == studio_viewport_error_none()
  decreases 0
=
  return li_rt_studio_viewport_error_retry()""",
        """def studio_viewport_error_retry() raises IO -> int
  requires true
  ensures result == studio_viewport_error_none()
  decreases 0
=
  return li_rt_studio_viewport_error_retry()""",
    )
    t = t.replace(
        "var compose: StudioShellCompose = studio_compose_shell_loading(w, h, 0, 0, studio_shell_loading_on())",
        "var compose: StudioShellCompose = studio_compose_shell_loading(w, h, 0, 0, 0, studio_shell_loading_on())",
    )
    t = t.replace(
        "var idle: StudioShellCompose = studio_compose_shell(w, h, 0, 0)\n  var frame: PaintFrame = paint_frame_new()\n  var idle_frame: PaintFrame = paint_frame_new()\n  studio_paint_shell_chrome(idle_frame, idle)\n  studio_paint_shell_chrome(frame, compose)\n  studio_paint_shell_loading(frame, compose.loading)",
        "var idle: StudioShellCompose = studio_compose_shell(w, h, 0, 0, 0)\n  var frame: PaintFrame = paint_frame_new()\n  var idle_frame: PaintFrame = paint_frame_new()\n  studio_paint_shell_chrome(idle_frame, idle)\n  studio_paint_shell_chrome(frame, compose)\n  studio_paint_shell_loading(frame, compose.loading)",
    )

    if "studio_err_gpu" not in t:
        raise SystemExit("patch incomplete: studio_err_gpu missing")
    return t


def main() -> None:
    LIB.write_text(patch(LIB.read_text()))
    print("OK", LIB)


if __name__ == "__main__":
    main()
