#!/usr/bin/env python3
"""One-shot patch: packages/li-studio/src/lib.li passes lic check + smokes."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "packages/li-studio/src/lib.li"
BASE = "532cf31"

TIMELINE_EXTERN = """
# UX-02 — timeline playback mock.
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

"""


def patch(text: str) -> str:
    if "extern proc li_rt_studio_timeline_playing" not in text:
        text = text.replace(
            "\n\n# PH-AGENT — MCP tool ID table",
            "\n" + TIMELINE_EXTERN + "\n# PH-AGENT — MCP tool ID table",
            1,
        )

    text = text.replace(
        "extern proc li_rt_studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires true\n  decreases tool_id",
        "extern proc li_rt_studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires true\n  ensures result != \"\"\n  decreases tool_id",
    )
    text = text.replace(
        "def studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires studio_mcp_tool_id_valid(tool_id) == 1\n  ensures true\n",
        "def studio_mcp_tool_name(tool_id: int) raises IO, Alloc -> str\n  requires studio_mcp_tool_id_valid(tool_id) == 1\n  ensures result != \"\"\n",
    )

    for a, b in [
        ("def studio_viewport_error_kind() -> int", "def studio_viewport_error_kind() raises IO -> int"),
        (
            "def studio_viewport_error_set_mock(kind: int) -> int",
            "def studio_viewport_error_set_mock(kind: int) raises IO -> int",
        ),
        ("def studio_viewport_error_retry() -> int", "def studio_viewport_error_retry() raises IO -> int"),
    ]:
        text = text.replace(a, b)

    for name in (
        "studio_timeline_playing",
        "studio_timeline_toggle_play",
        "studio_timeline_tick_frame",
        "studio_timeline_reset_mock",
    ):
        text = text.replace(f"def {name}() -> int\n  requires true", f"def {name}() raises IO -> int\n  requires true", 1)
    text = text.replace(
        "def studio_timeline_playhead_pct() -> float\n  requires true\n  ensures result >= 0.0",
        "def studio_timeline_playhead_pct() raises IO -> float\n  requires true\n  ensures result >= 0.0",
        1,
    )

    row0_y = """  var row0: int = row
  var y: float = insp_y + studio_inspector_header_height_px() + pad
  if row0 == 1:
    y = y + row_h + gap"""
    row0_panel = """  var row0: int = row
  var y: float = panel.y + pad
  if row0 == 1:
    y = y + row_h + gap"""

    text = text.replace(
        "def studio_inspector_field_at(insp_x: float, insp_y: float, insp_w: float, row: int) -> StudioInspectorField\n  requires insp_w >= studio_inspector_field_label_width_px()\n  requires row >= 0\n  requires row <= 1\n  decreases row",
        "def studio_inspector_field_at(insp_x: float, insp_y: float, insp_w: float, row: int) -> StudioInspectorField\n  requires insp_w >= studio_inspector_field_label_width_px()\n  requires row >= 0\n  requires row <= 1\n  ensures result.label_rect.w == studio_inspector_field_label_width_px()\n  ensures result.value_rect.w >= 0.0\n  decreases row",
    )

    text = text.replace(
        "  var y: float = insp_y + studio_inspector_header_height_px() + pad + (row * (row_h + gap))",
        row0_y,
    )
    text = text.replace(
        "  var y: float = panel.y + pad + (row * (row_h + gap))",
        row0_panel,
    )

    text = text.replace(
        "  out.label_rect = rect_make(insp_x + pad, y, label_w, row_h)\n  out.value_rect = rect_make(insp_x + pad + label_w + pad, y, value_w, row_h)",
        """  var lx: float = insp_x + pad
  var ly: float = y
  var lw: float = label_w
  var lh0: float = row_h
  var vx: float = insp_x + pad + label_w + pad
  var vy: float = y
  var lh1: float = row_h
  out.label_rect = rect_make(lx, ly, lw, lh0)
  out.value_rect = rect_make(vx, vy, value_w, lh1)""",
    )

    text = text.replace(
        """  out.header_rect = studio_inspector_header_rect_at(insp_x, insp_y, insp_w)
  out.has_selection = 1
  out.field_rows = studio_inspector_field_rows()
  out.field_count = studio_inspector_selection_field_count()
  out.field0 = studio_inspector_field_at(insp_x, insp_y, insp_w, 0)
  out.field1 = studio_inspector_field_at(insp_x, insp_y, insp_w, 1)""",
        """  var hx: float = lay.inspector.x
  var hy: float = lay.inspector.y
  var hw: float = lay.inspector.w
  out.header_rect = studio_inspector_header_rect_at(hx, hy, hw)
  out.has_selection = 1
  out.field_rows = studio_inspector_field_rows()
  out.field_count = studio_inspector_selection_field_count()
  var f0x: float = lay.inspector.x
  var f0y: float = lay.inspector.y
  var f0w: float = lay.inspector.w
  var f1x: float = lay.inspector.x
  var f1y: float = lay.inspector.y
  var f1w: float = lay.inspector.w
  out.field0 = studio_inspector_field_at(f0x, f0y, f0w, 0)
  out.field1 = studio_inspector_field_at(f1x, f1y, f1w, 1)""",
    )

    text = text.replace(
        "  out.playhead_rect = studio_timeline_playhead_rect_at(out.track_rect.x, out.track_rect.y, out.track_rect.w, out.track_rect.h, pct0)\n  out.playhead_pct = pct0",
        """  var pct1: float = studio_timeline_playhead_pct()
  out.playhead_rect = studio_timeline_playhead_rect_at(out.track_rect.x, out.track_rect.y, out.track_rect.w, out.track_rect.h, pct0)
  out.playhead_pct = pct1""",
    )

    text = text.replace(
        """  out.hint_line1_rect = studio_inspector_empty_hint_line_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w, 0)
  out.hint_line2_rect = studio_inspector_empty_hint_line_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w, 1)
  out.hint_box_rect = studio_inspector_empty_hint_box_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w, lay.inspector.h)""",
        """  var h1x: float = layout.inspector.x
  var h1y: float = layout.inspector.y
  var h1w: float = layout.inspector.w
  var h2x: float = layout.inspector.x
  var h2y: float = layout.inspector.y
  var h2w: float = layout.inspector.w
  var hbx: float = layout.inspector.x
  var hby: float = layout.inspector.y
  var hbw: float = layout.inspector.w
  var hbh: float = layout.inspector.h
  out.hint_line1_rect = studio_inspector_empty_hint_line_rect_at(h1x, h1y, h1w, 0)
  out.hint_line2_rect = studio_inspector_empty_hint_line_rect_at(h2x, h2y, h2w, 1)
  out.hint_box_rect = studio_inspector_empty_hint_box_rect_at(hbx, hby, hbw, hbh)""",
    )

    text = text.replace(
        "studio_inspector_empty_hint_line_rect_at(layout.inspector.x, lay.inspector.y, lay.inspector.w,",
        "studio_inspector_empty_hint_line_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w,",
    )
    text = text.replace(
        "studio_inspector_empty_hint_box_rect_at(layout.inspector.x, lay.inspector.y, lay.inspector.w, layout.inspector.h)",
        "studio_inspector_empty_hint_box_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w, layout.inspector.h)",
    )

    if "out.tool_request = studio_agent_tool_request_none()" not in text:
        text = text.replace(
            "    out.retry_hint_rect = studio_agent_retry_hint_rect_at(out.error_strip_rect)\n  return out\n\ndef studio_compose_agent_chrome_with_tool",
            "    out.retry_hint_rect = studio_agent_retry_hint_rect_at(out.error_strip_rect)\n  out.tool_request = studio_agent_tool_request_none()\n  return out\n\ndef studio_compose_agent_chrome_with_tool",
            1,
        )

    old = """  var layout: StudioShellLayout = layout_studio_shell_adaptive(w, h)
  out.layout = layout
  out.config = studio_project_config_new(active_profile)
  out.dock.rect = layout.dock"""
    new = """  var layout: StudioShellLayout = layout_studio_shell_adaptive(w, h)
  out.layout = layout
  var lay: StudioShellLayout = out.layout
  out.config = studio_project_config_new(active_profile)
  out.dock.rect = lay.dock"""
    if old in text:
        text = text.replace(old, new)
        text = text.replace(
            "studio_dock_slot_rect_at(layout.dock.x, layout.dock.y, layout.dock.w, layout.dock.h, slot_dock)\n  out.outliner = studio_compose_outliner(layout.dock, slot_outliner)\n  out.timeline.rect = layout.timeline\n  out.timeline.track_rect = studio_timeline_track_rect_at(layout.timeline.x, layout.timeline.y, layout.timeline.w, layout.timeline.h)",
            "studio_dock_slot_rect_at(lay.dock.x, lay.dock.y, lay.dock.w, lay.dock.h, slot_dock)\n  out.outliner = studio_compose_outliner(lay.dock, slot_outliner)\n  out.timeline.rect = lay.timeline\n  out.timeline.track_rect = studio_timeline_track_rect_at(lay.timeline.x, lay.timeline.y, lay.timeline.w, lay.timeline.h)",
        )
        text = text.replace(
            "  out.inspector.rect = layout.inspector\n  out.inspector.header_rect = studio_inspector_header_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w)",
            "  out.inspector.rect = lay.inspector\n  out.inspector.header_rect = studio_inspector_header_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w)",
        )
        text = text.replace(
            "studio_inspector_empty_hint_line_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w,",
            "studio_inspector_empty_hint_line_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w,",
        )
        text = text.replace(
            "studio_inspector_empty_hint_box_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w, layout.inspector.h)",
            "studio_inspector_empty_hint_box_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w, lay.inspector.h)",
        )
        text = text.replace(
            "studio_inspector_field_row_rect_at(layout.inspector.x, layout.inspector.y, layout.inspector.w)",
            "studio_inspector_field_row_rect_at(lay.inspector.x, lay.inspector.y, lay.inspector.w)",
        )
        text = text.replace(
            "  out.viewport_empty.frame_rect = layout.viewport\n  out.viewport_empty.empty_visible = 1\n  out.viewport_empty.title_rect = studio_viewport_empty_title_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)\n  out.viewport_empty.cta_rect = studio_viewport_empty_cta_rect_at(layout.viewport.x, layout.viewport.y, layout.viewport.w, layout.viewport.h)\n  out.viewport_error = studio_compose_viewport_error_overlay(layout, studio_viewport_error_kind())\n  out.agent = studio_compose_agent_chrome(layout, studio_agent_task_idle(), studio_agent_context_none())\n  out.palette = studio_compose_palette(layout.viewport_w, layout.viewport_h, studio_palette_closed_flag())",
            "  out.viewport_empty.frame_rect = lay.viewport\n  out.viewport_empty.empty_visible = 1\n  out.viewport_empty.title_rect = studio_viewport_empty_title_rect_at(lay.viewport.x, lay.viewport.y, lay.viewport.w, lay.viewport.h)\n  out.viewport_empty.cta_rect = studio_viewport_empty_cta_rect_at(lay.viewport.x, lay.viewport.y, lay.viewport.w, lay.viewport.h)\n  var lay_err: StudioShellLayout = lay\n  out.viewport_error = studio_compose_viewport_error_overlay(lay_err, studio_viewport_error_kind())\n  var lay_ag: StudioShellLayout = lay\n  out.agent = studio_compose_agent_chrome(lay_ag, studio_agent_task_idle(), studio_agent_context_none())\n  out.palette = studio_compose_palette(lay.viewport_w, lay.viewport_h, studio_palette_closed_flag())",
        )

    text = text.replace(
        "studio_compose_shell_loading(w, h, 0, 0, studio_shell_loading_on())",
        "studio_compose_shell_loading(w, h, 0, 0, 0, studio_shell_loading_on())",
    )
    text = text.replace(
        "def studio_shell_loading_frame(w: float, h: float) -> PaintFrame\n  requires w > 0.0\n  requires h > 0.0\n  ensures result.cmd_count > 0\n  decreases 0\n=\n  var compose: StudioShellCompose = studio_compose_shell_loading(w, h, 0, 0, 0, studio_shell_loading_on())\n  var idle: StudioShellCompose = studio_compose_shell(w, h, 0, 0)",
        "def studio_shell_loading_frame(w: float, h: float) -> PaintFrame\n  requires w > 0.0\n  requires h > 0.0\n  ensures result.cmd_count > 0\n  decreases 0\n=\n  var w0: float = w\n  var h0: float = h\n  var w1: float = w\n  var h1: float = h\n  var compose: StudioShellCompose = studio_compose_shell_loading(w0, h0, 0, 0, 0, studio_shell_loading_on())\n  var idle: StudioShellCompose = studio_compose_shell(w1, h1, 0, 0, 0)",
    )
    text = text.replace(
        "var idle: StudioShellCompose = studio_compose_shell(w, h, 0, 0)",
        "var idle: StudioShellCompose = studio_compose_shell(w, h, 0, 0, 0)",
    )

    return text


def main() -> int:
    raw = subprocess.check_output(
        ["git", "show", f"{BASE}:packages/li-studio/src/lib.li"],
        cwd=ROOT,
        text=True,
    )
    LIB.write_text(patch(raw))
    print(f"wrote {LIB} ({len(LIB.read_text().splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
