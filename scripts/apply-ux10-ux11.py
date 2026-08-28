#!/usr/bin/env python3
"""Apply UX-10 (a11y focus ring) and UX-11 (shell loading skeleton) to li-ui + li-studio."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "packages/li-ui/src/lib.li"
ST = ROOT / "packages/li-studio/src/lib.li"

UI_A11Y = """
# UX-10 — focus_ring in docs/design/studio-design-tokens.toml (#3dd6ff)
def studio_color_focus_ring() -> Color
  requires true
  ensures result.r >= 0.0
  decreases 0
=
  return color_rgb(0.239, 0.839, 1.0, 1.0)

def studio_focus_ring_stroke_px() -> float
  requires true
  ensures result == 2.0
  decreases 0
=
  return 2.0

# UX-10 contrast stub — real ratio from host when text/background samples ship
def studio_contrast_ratio_ok() -> float
  requires true
  ensures result >= 1.0
  decreases 0
=
  return 1.0

# UX-11 skeleton fills — muted elevated (no spinner paint IR)
def studio_color_skeleton_muted() -> Color
  requires true
  ensures result.r >= 0.0
  decreases 0
=
  return color_rgb(0.118, 0.145, 0.180, 1.0)

def studio_color_skeleton_highlight() -> Color
  requires true
  ensures result.r >= 0.0
  decreases 0
=
  return color_rgb(0.145, 0.173, 0.212, 1.0)

def studio_paint_focus_ring(frame: var PaintFrame, region_rect: Rect) -> unit
  requires frame.cmd_count >= 0
  requires region_rect.w >= 0.0
  requires region_rect.h >= 0.0
  ensures frame.cmd_count == old(frame.cmd_count) + 1
  ensures frame.last_kind == paint_op_stroke_rect()
  ensures frame.last_rect.w == region_rect.w
  decreases 0
=
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_stroke_rect()
  frame.last_rect = region_rect
  frame.last_color = studio_color_focus_ring()

"""

LOADING_TYPE = """
# UX-11 — shell loading / skeleton rects (viewport + inspector); no spinner IR
type StudioShellLoadingState = object
  public shell_loading: int
  public viewport_skeleton: Rect
  public inspector_skeleton: Rect
  public inspector_header_skeleton: Rect
  public inspector_field_skeleton: Rect
  public inspector_field2_skeleton: Rect

"""

STUDIO_HELPERS = """
def studio_shell_loading_on() -> int
  requires true
  ensures result == 1
  decreases 0
=
  return 1

def studio_shell_loading_off() -> int
  requires true
  ensures result == 0
  decreases 0
=
  return 0

def studio_shell_loading_state_new() -> StudioShellLoadingState
  requires true
  ensures result.shell_loading == studio_shell_loading_off()
  decreases 0
=
  var s: StudioShellLoadingState
  s.shell_loading = studio_shell_loading_off()
  s.viewport_skeleton = rect_make(0.0, 0.0, 0.0, 0.0)
  s.inspector_skeleton = rect_make(0.0, 0.0, 0.0, 0.0)
  s.inspector_header_skeleton = rect_make(0.0, 0.0, 0.0, 0.0)
  s.inspector_field_skeleton = rect_make(0.0, 0.0, 0.0, 0.0)
  s.inspector_field2_skeleton = rect_make(0.0, 0.0, 0.0, 0.0)
  return s

def studio_skeleton_viewport_rect(viewport: Rect) -> Rect
  requires viewport.w >= 0.0
  requires viewport.h >= 0.0
  ensures result.w >= 0.0
  ensures result.h >= 0.0
  decreases 0
=
  var pad: float = 24.0
  var w: float = viewport.w - (pad * 2.0)
  var h: float = viewport.h - (pad * 2.0)
  if w < 0.0:
    w = 0.0
  if h < 0.0:
    h = 0.0
  return rect_make(viewport.x + pad, viewport.y + pad, w, h)

def studio_skeleton_inspector_panel_rect(inspector: Rect) -> Rect
  requires inspector.w >= 0.0
  requires inspector.h >= studio_inspector_header_height_px()
  ensures result.w == inspector.w
  decreases 0
=
  var y: float = inspector.y + studio_inspector_header_height_px()
  var h: float = inspector.h - studio_inspector_header_height_px()
  if h < 0.0:
    h = 0.0
  return rect_make(inspector.x, y, inspector.w, h)

def studio_skeleton_inspector_header_rect(inspector: Rect) -> Rect
  requires inspector.w >= 0.0
  ensures result.h == studio_inspector_header_height_px()
  decreases 0
=
  return rect_make(inspector.x, inspector.y, inspector.w, studio_inspector_header_height_px())

def studio_skeleton_inspector_field_rect(panel: Rect, row: int) -> Rect
  requires panel.w >= 0.0
  requires panel.h >= 0.0
  requires row >= 0
  requires row <= 1
  ensures result.w >= 0.0
  decreases row
=
  var pad: float = 12.0
  var row_h: float = 20.0
  var gap: float = 8.0
  var y: float = panel.y + pad + (row * (row_h + gap))
  var w: float = panel.w - (pad * 2.0)
  if w < 0.0:
    w = 0.0
  return rect_make(panel.x + pad, y, w, row_h)

def studio_compose_shell_loading_rects(layout: StudioShellLayout) -> StudioShellLoadingState
  requires layout.viewport_w > 0.0
  ensures result.shell_loading == studio_shell_loading_on()
  decreases 0
=
  var out: StudioShellLoadingState = studio_shell_loading_state_new()
  out.shell_loading = studio_shell_loading_on()
  out.viewport_skeleton = studio_skeleton_viewport_rect(layout.viewport)
  out.inspector_skeleton = studio_skeleton_inspector_panel_rect(layout.inspector)
  out.inspector_header_skeleton = studio_skeleton_inspector_header_rect(layout.inspector)
  out.inspector_field_skeleton = studio_skeleton_inspector_field_rect(out.inspector_skeleton, 0)
  out.inspector_field2_skeleton = studio_skeleton_inspector_field_rect(out.inspector_skeleton, 1)
  return out

def studio_region_rect_for_focus(layout: StudioShellLayout, region: int) -> Rect
  requires region >= studio_region_dock()
  requires region <= studio_region_agent_strip()
  ensures result.w >= 0.0
  decreases region
=
  if region == studio_region_dock():
    return layout.dock
  if region == studio_region_topbar():
    return layout.topbar
  if region == studio_region_viewport():
    return layout.viewport
  if region == studio_region_inspector():
    return layout.inspector
  if region == studio_region_timeline():
    return layout.timeline
  return layout.agent_strip

def studio_paint_focus_ring_cmds(active_region: int) -> int
  requires active_region >= studio_region_dock()
  requires active_region <= studio_region_agent_strip()
  ensures result >= 0
  ensures result <= 1
  decreases active_region
=
  if active_region == studio_region_viewport():
    return 0
  return 1

def studio_paint_focus_ring_for_panel(frame: var PaintFrame, compose: StudioShellCompose) -> unit
  requires frame.cmd_count >= 0
  requires compose.panel.active_region >= studio_region_dock()
  requires compose.panel.active_region <= studio_region_agent_strip()
  ensures frame.cmd_count == old(frame.cmd_count) + studio_paint_focus_ring_cmds(compose.panel.active_region)
  decreases compose.panel.active_region
=
  if studio_paint_focus_ring_cmds(compose.panel.active_region) == 0:
    return
  var region_rect: Rect = studio_region_rect_for_focus(compose.layout, compose.panel.active_region)
  studio_paint_focus_ring(frame, region_rect)

def studio_paint_shell_loading_cmds() -> int
  requires true
  ensures result == 4
  decreases 0
=
  return 4

def studio_paint_shell_loading(frame: var PaintFrame, loading: StudioShellLoadingState) -> unit
  requires frame.cmd_count >= 0
  requires loading.shell_loading >= studio_shell_loading_off()
  requires loading.shell_loading <= studio_shell_loading_on()
  ensures frame.cmd_count == old(frame.cmd_count) + studio_paint_shell_loading_cmds(loading.shell_loading)
  decreases loading.shell_loading
=
  if loading.shell_loading != studio_shell_loading_on():
    return
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = loading.viewport_skeleton
  frame.last_color = studio_color_skeleton_muted()
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = loading.inspector_header_skeleton
  frame.last_color = studio_color_skeleton_highlight()
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = loading.inspector_field_skeleton
  frame.last_color = studio_color_skeleton_muted()
  frame.cmd_count = frame.cmd_count + 1
  frame.last_kind = paint_op_fill_rect()
  frame.last_rect = loading.inspector_field2_skeleton
  frame.last_color = studio_color_skeleton_highlight()

"""


def patch_ui(text: str) -> str:
    text = re.sub(
        r"(def li_std_ui_studio_composables_version\(\) -> int\n  requires true\n  ensures result == )\d+",
        r"\g<1>5",
        text,
        count=1,
    )
    text = re.sub(
        r"(def li_std_ui_studio_composables_version\(\) -> int\n  requires true\n  ensures result == 5\n  decreases 0\n=\n  return )\d+",
        r"\g<1>5",
        text,
        count=1,
    )
    if "studio_color_focus_ring" not in text:
        anchor = (
            "def studio_color_border() -> Color\n"
            "  requires true\n"
            "  ensures result.r >= 0.0\n"
            "  decreases 0\n"
            "=\n"
            "  return color_rgb(0.188, 0.212, 0.239, 1.0)\n"
            "\n"
            "def studio_color_accent_amber()"
        )
        if anchor not in text:
            raise SystemExit("li-ui: studio_color_border anchor missing")
        text = text.replace(anchor, anchor.replace("def studio_color_accent_amber()", UI_A11Y + "def studio_color_accent_amber()", 1))
    return text


def patch_studio(text: str) -> str:
    text = re.sub(
        r"def li_std_studio_version\(\) -> int\n  requires true\n  ensures result == \d+\n  decreases 0\n=\n  return \d+",
        "def li_std_studio_version() -> int\n  requires true\n  ensures result == 5\n  decreases 0\n=\n  return 5",
        text,
        count=1,
    )
    if "StudioShellLoadingState" not in text:
        m = re.search(r"type StudioShellCompose = object\n(?:  public \w+:.*\n)+", text)
        if not m:
            raise SystemExit("li-studio: StudioShellCompose block not found")
        block = m.group(0)
        if "public loading:" in block:
            raise SystemExit("li-studio: loading already on compose")
        new_block = block.rstrip() + "\n  public loading: StudioShellLoadingState\n\n"
        text = text.replace(block, LOADING_TYPE + new_block + STUDIO_HELPERS, 1)

    # init loading on every shell compose return
    if "out.loading = studio_shell_loading_state_new()" not in text:
        text = text.replace(
            "  out.panel = gui_panel_state_new()\n  return out",
            "  out.panel = gui_panel_state_new()\n  out.loading = studio_shell_loading_state_new()\n  return out",
        )

    if "def studio_compose_shell_loading(" not in text:
        insert_before = "\ndef studio_compose_shell_palette"
        if insert_before not in text:
            insert_before = "\ndef studio_compose_shell_agent"
        if insert_before not in text:
            raise SystemExit("li-studio: compose return anchor missing")
        loading_fn = """
def studio_compose_shell_loading(w: float, h: float, active_dock_slot: int, has_selection: int, shell_loading: int) -> StudioShellCompose
  requires w > 0.0
  requires h > 0.0
  requires active_dock_slot >= 0
  requires active_dock_slot < studio_dock_slot_count()
  requires has_selection >= 0
  requires has_selection <= 1
  requires shell_loading >= studio_shell_loading_off()
  requires shell_loading <= studio_shell_loading_on()
  ensures result.loading.shell_loading == shell_loading
  decreases shell_loading
=
  var out: StudioShellCompose = studio_compose_shell(w, h, active_dock_slot, has_selection)
  if shell_loading == studio_shell_loading_on():
    out.loading = studio_compose_shell_loading_rects(out.layout)
  return out

"""
        # scene_entity_count overload
        if "scene_entity_count: int) -> StudioShellCompose" in text and "studio_compose_shell_loading(w: float, h: float, active_dock_slot: int, has_selection: int, scene_entity_count: int" not in text:
            loading_fn = loading_fn.replace(
                "studio_compose_shell(w, h, active_dock_slot, has_selection)",
                "studio_compose_shell(w, h, active_dock_slot, has_selection, scene_entity_count)",
            ).replace(
                "has_selection: int, shell_loading: int",
                "has_selection: int, scene_entity_count: int, shell_loading: int",
            ).replace(
                "requires shell_loading <= studio_shell_loading_on()",
                "requires scene_entity_count >= 0\n  requires shell_loading <= studio_shell_loading_on()",
            )
        text = text.replace(insert_before, "\n" + loading_fn + insert_before, 1)

    if "def studio_shell_loading_frame(" not in text:
        m = re.search(
            r"def studio_shell_frame\(w: float, h: float\) -> PaintFrame\n.*?\n  return frame\n",
            text,
            re.S,
        )
        if not m:
            raise SystemExit("li-studio: studio_shell_frame not found")
        old = m.group(0)
        agent_args = "studio_compose_shell_agent(w, h, 0, 1, studio_agent_task_running())"
        if "scene_entity_count" in old:
            agent_args = "studio_compose_shell_agent(w, h, 0, 1, 1, studio_agent_task_running())"
            loading_call = "studio_compose_shell_loading(w, h, 0, 0, 0, studio_shell_loading_on())"
            idle_call = "studio_compose_shell(w, h, 0, 0, 0)"
        else:
            loading_call = "studio_compose_shell_loading(w, h, 0, 0, studio_shell_loading_on())"
            idle_call = "studio_compose_shell(w, h, 0, 0)"
        extra = f"""

def studio_shell_loading_frame(w: float, h: float) -> PaintFrame
  requires w > 0.0
  requires h > 0.0
  ensures result.cmd_count > 0
  decreases 0
=
  var compose: StudioShellCompose = {loading_call}
  var idle: StudioShellCompose = {idle_call}
  var frame: PaintFrame = paint_frame_new()
  var idle_frame: PaintFrame = paint_frame_new()
  studio_paint_shell_chrome(idle_frame, idle)
  studio_paint_shell_chrome(frame, compose)
  studio_paint_shell_loading(frame, compose.loading)
  return frame
"""
        text = text.replace(old, old + extra, 1)

    return text


def main() -> None:
    ui_text = patch_ui(UI.read_text())
    UI.write_text(ui_text)
    st_text = patch_studio(ST.read_text())
    ST.write_text(st_text)
    assert "studio_color_focus_ring" in ui_text
    assert "StudioShellLoadingState" in st_text
    assert "studio_paint_focus_ring_for_panel" in st_text
    print("OK: UX-10/11 applied to li-ui and li-studio")


if __name__ == "__main__":
    main()
