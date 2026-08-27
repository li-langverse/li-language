#!/usr/bin/env python3
"""Patch packages/li-studio/src/lib.li for UX-11 agent invoke compose rects."""
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "packages/li-studio/src/lib.li"
text = p.read_text()
text = text.replace(
    "def li_std_studio_version() -> int\n  requires true\n  ensures result == 6\n  decreases 0\n=\n  return 6",
    "def li_std_studio_version() -> int\n  requires true\n  ensures result == 7\n  decreases 0\n=\n  return 7",
    1,
)
if "studio_agent_send_btn_width_px" not in text:
    text = text.replace(
        "def studio_agent_retry_hint_width_px() -> float\n  requires true\n  ensures result == 88.0\n  decreases 0\n=\n  return 88.0\n\ndef studio_agent_last_action_reversible",
        "def studio_agent_retry_hint_width_px() -> float\n  requires true\n  ensures result == 88.0\n  decreases 0\n=\n  return 88.0\n\ndef studio_agent_send_btn_width_px() -> float\n  requires true\n  ensures result == 48.0\n  decreases 0\n=\n  return 48.0\n\ndef studio_agent_tool_trace_width_px() -> float\n  requires true\n  ensures result == 140.0\n  decreases 0\n=\n  return 140.0\n\ndef studio_agent_last_action_reversible",
    )
if "public task_input_rect" not in text:
    text = text.replace(
        "  public tool_request: StudioAgentToolRequest\n\ntype StudioProjectConfig",
        "  public tool_request: StudioAgentToolRequest\n  public task_input_rect: Rect\n  public send_rect: Rect\n  public tool_trace_rect: Rect\n  public invoke_input_visible: int\n  public tool_trace_visible: int\n\ntype StudioProjectConfig",
    )
block = r'''
def studio_agent_invoke_input_visible(task_state: int) -> int
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result >= 0
  ensures result <= 1
  decreases task_state
=
  if task_state == studio_agent_task_idle():
    return 1
  return 0

def studio_agent_tool_trace_visible(task_state: int) -> int
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result >= 0
  ensures result <= 1
  decreases task_state
=
  if task_state == studio_agent_task_running():
    return 1
  return 0

def studio_agent_tool_trace_visible_for(task_state: int, tool_id: int) -> int
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result >= 0
  ensures result <= 1
  decreases task_state
=
  if studio_agent_tool_trace_visible(task_state) != 1:
    return 0
  if studio_mcp_tool_id_valid(tool_id) != 1:
    return 0
  return 1

def studio_agent_send_rect_at(strip_x: float, strip_y: float, strip_w: float, strip_h: float, cancel_visible: int) -> Rect
  requires strip_w >= studio_agent_send_btn_width_px()
  requires strip_h >= 0.0
  requires cancel_visible >= 0
  requires cancel_visible <= 1
  ensures result.w == studio_agent_send_btn_width_px()
  decreases cancel_visible
=
  var pad: float = studio_agent_status_pad_px()
  var reserved: float = studio_agent_send_btn_width_px() + pad
  if cancel_visible == 1:
    reserved = reserved + studio_agent_cancel_btn_width_px() + pad
  var x: float = strip_x + strip_w - reserved
  var btn_h: float = strip_h - (pad * 2.0)
  if btn_h < 20.0:
    btn_h = 20.0
  if btn_h > strip_h:
    btn_h = strip_h
  var y: float = strip_y + (strip_h - btn_h) / 2.0
  return rect_make(x, y, studio_agent_send_btn_width_px(), btn_h)

def studio_agent_task_input_rect_at(strip_x: float, strip_y: float, strip_w: float, strip_h: float, send_rect: Rect) -> Rect
  requires strip_w >= studio_agent_send_btn_width_px()
  requires strip_h >= 0.0
  requires send_rect.w == studio_agent_send_btn_width_px()
  ensures result.w >= 0.0
  decreases 0
=
  var pad: float = studio_agent_status_pad_px()
  var x: float = strip_x + pad
  var w: float = send_rect.x - x - pad
  if w < 0.0:
    w = 0.0
  var h: float = strip_h - (pad * 2.0)
  if h < 20.0:
    h = 20.0
  if h > strip_h:
    h = strip_h
  var y: float = strip_y + (strip_h - h) / 2.0
  return rect_make(x, y, w, h)

def studio_agent_tool_trace_rect_at(strip_x: float, strip_y: float, strip_w: float, strip_h: float, cancel_visible: int) -> Rect
  requires strip_w >= studio_agent_tool_trace_width_px()
  requires strip_h >= 0.0
  requires cancel_visible >= 0
  requires cancel_visible <= 1
  ensures result.w == studio_agent_tool_trace_width_px()
  decreases cancel_visible
=
  var pad: float = studio_agent_status_pad_px()
  var reserved: float = studio_agent_tool_trace_width_px() + pad
  if cancel_visible == 1:
    reserved = reserved + studio_agent_cancel_btn_width_px() + pad
  var x: float = strip_x + strip_w - reserved
  var h: float = strip_h - (pad * 2.0)
  if h < 18.0:
    h = 18.0
  if h > strip_h:
    h = strip_h
  var y: float = strip_y + (strip_h - h) / 2.0
  return rect_make(x, y, studio_agent_tool_trace_width_px(), h)

def studio_compose_agent_invoke_fields(out: StudioAgentChromeCompose, layout: StudioShellLayout, task_state: int, tool_id: int) -> StudioAgentChromeCompose
  requires task_state >= studio_agent_task_idle()
  requires task_state <= studio_agent_task_done()
  ensures result.invoke_input_visible == studio_agent_invoke_input_visible(task_state)
  decreases task_state
=
  var agent: StudioAgentChromeCompose = out
  var ts_invoke: int = task_state
  var ts_trace: int = task_state
  agent.invoke_input_visible = studio_agent_invoke_input_visible(ts_invoke)
  agent.tool_trace_visible = studio_agent_tool_trace_visible_for(ts_trace, tool_id)
  agent.task_input_rect = rect_make(0.0, 0.0, 0.0, 0.0)
  agent.send_rect = rect_make(0.0, 0.0, 0.0, 0.0)
  agent.tool_trace_rect = rect_make(0.0, 0.0, 0.0, 0.0)
  if agent.invoke_input_visible == 1:
    agent.send_rect = studio_agent_send_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h, agent.cancel_visible)
    agent.task_input_rect = studio_agent_task_input_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h, agent.send_rect)
  if agent.tool_trace_visible == 1:
    agent.tool_trace_rect = studio_agent_tool_trace_rect_at(layout.agent_strip.x, layout.agent_strip.y, layout.agent_strip.w, layout.agent_strip.h, agent.cancel_visible)
  return agent

'''
if "studio_agent_invoke_input_visible" not in text:
    text = text.replace(
        "  return rect_make(x, error_strip.y, studio_agent_retry_hint_width_px(), error_strip.h)\n\ndef studio_compose_agent_progress",
        "  return rect_make(x, error_strip.y, studio_agent_retry_hint_width_px(), error_strip.h)\n" + block + "\ndef studio_compose_agent_progress",
    )
text = text.replace(
    "  ensures result <= 7\n  decreases task_state\n=\n  var ctx0: int = agent_context_label\n  var ts_cancel: int = task_state\n  var ts_progress: int = task_state\n  var ts_error: int = task_state\n  var ts_retry: int = task_state\n  var n: int = 2",
    "  ensures result <= 9\n  decreases task_state\n=\n  var ctx0: int = agent_context_label\n  var ts_cancel: int = task_state\n  var ts_progress: int = task_state\n  var ts_error: int = task_state\n  var ts_retry: int = task_state\n  var ts_invoke: int = task_state\n  var ts_trace: int = task_state\n  var n: int = 2",
)
if "studio_agent_invoke_input_visible(ts_invoke)" not in text:
    text = text.replace(
        "  if studio_agent_retry_visible(ts_retry) == 1:\n    n = n + 1\n  return n\n\ndef studio_compose_agent_chrome",
        "  if studio_agent_retry_visible(ts_retry) == 1:\n    n = n + 1\n  if studio_agent_invoke_input_visible(ts_invoke) == 1:\n    n = n + 2\n  if studio_agent_tool_trace_visible(ts_trace) == 1:\n    n = n + 1\n  return n\n\ndef studio_compose_agent_chrome",
    )
text = text.replace(
    "  out.tool_request = studio_agent_tool_request_none()\n  return out\n\ndef studio_compose_agent_chrome_with_tool",
    "  out.tool_request = studio_agent_tool_request_none()\n  return studio_compose_agent_invoke_fields(out, layout, task_state, studio_mcp_tool_unknown())\n\ndef studio_compose_agent_chrome_with_tool",
)
text = text.replace(
    "  out.tool_request = tool_request\n  return out\n\ndef studio_compose_shell_profile",
    "  out.tool_request = tool_request\n  return studio_compose_agent_invoke_fields(out, layout, task_state, tool_request.tool_id)\n\ndef studio_compose_shell_profile",
)
if "agent.invoke_input_visible == 1" not in text:
    text = text.replace(
        "  if agent.retry_visible == 1:\n    frame.last_kind = paint_op_stroke_rect()\n    frame.last_rect = agent.retry_hint_rect\n    frame.last_color = studio_color_accent_cyan()\n\ndef studio_paint_inspector",
        "  if agent.retry_visible == 1:\n    frame.last_kind = paint_op_stroke_rect()\n    frame.last_rect = agent.retry_hint_rect\n    frame.last_color = studio_color_accent_cyan()\n  if agent.invoke_input_visible == 1:\n    frame.last_kind = paint_op_stroke_rect()\n    frame.last_rect = agent.task_input_rect\n    frame.last_color = studio_color_border()\n    frame.last_kind = paint_op_stroke_rect()\n    frame.last_rect = agent.send_rect\n    frame.last_color = studio_color_accent_cyan()\n  if agent.tool_trace_visible == 1:\n    frame.last_kind = paint_op_stroke_rect()\n    frame.last_rect = agent.tool_trace_rect\n    frame.last_color = studio_color_accent_cyan()\n\ndef studio_paint_inspector",
    )
if "agent.invoke_input_visible == 1:\n    if agent.send_rect.w" not in text:
    text = text.replace(
        "  if studio_agent_context_visible(agent.agent_context_label) == 1:\n    if agent.context_rect.w != studio_agent_context_label_width_px():\n      return 0\n  return 1\n\ndef studio_panel_switch_inspector",
        "  if studio_agent_context_visible(agent.agent_context_label) == 1:\n    if agent.context_rect.w != studio_agent_context_label_width_px():\n      return 0\n  if agent.invoke_input_visible == 1:\n    if agent.send_rect.w != studio_agent_send_btn_width_px():\n      return 0\n    if agent.task_input_rect.w <= 0.0:\n      return 0\n  if agent.tool_trace_visible == 1:\n    if agent.tool_trace_rect.w != studio_agent_tool_trace_width_px():\n      return 0\n  return 1\n\ndef studio_panel_switch_inspector",
    )
p.write_text(text)
print("ok")
