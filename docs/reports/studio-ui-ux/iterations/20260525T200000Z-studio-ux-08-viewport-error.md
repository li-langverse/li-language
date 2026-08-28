# Studio UX iteration — UX-08 viewport error recovery

**Date:** 2026-05-25  
**Branch:** `feat/studio-gap-close-wave1`  
**Scope:** `packages/li-studio/`

## Summary

Viewport GPU / missing-asset recovery overlay in Li compose + paint IR with honest runtime mock (no wgpu surface failure detection).

## UX scores

| ID | Score | Note |
|----|-------|------|
| UX-08 | **2.8** | GPU + missing-asset kinds, message + retry stroke rects, retry clears mock; native wgpu probe still gap (Primer = 3). |

## Evidence

- `packages/li-studio/src/lib.li` — `StudioViewportErrorOverlay`, `studio_paint_viewport_error`, shell chrome branches error before empty/scene.
- `runtime/li_rt.c` — `g_studio_viewport_error_kind` mock.
- `packages/li-studio/li-tests/smoke/studio_viewport_error.li` — gpu overlay cmds + retry clears.

## Native gap (honest)

Li IR does not call wgpu `surface_ok` or asset loaders; hosts set `li_rt_studio_viewport_error_set_mock` until render bridge reports real failures.
