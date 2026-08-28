# Release notes: 2026-05-25 — studio-ux08-viewport-error-lib

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-UX, UX-08  
**Author:** agent

---

## Summary (one sentence)

Wires UX-08 viewport error recovery into `packages/li-studio/src/lib.li` (compose overlay, shell chrome counts, paint path) matching existing `li_rt_studio_viewport_error_*` runtime and `studio_viewport_error.li` smoke.

## Agent continuation (required)

1. Read: `packages/li-studio/src/lib.li` (UX-08 section), `packages/li-studio/li-tests/smoke/studio_viewport_error.li`.
2. Run: `lic check packages/li-studio/src/lib.li`; `lic check packages/li-studio/li-tests/smoke/studio_viewport_error.li`.
3. Then: SDL/native host should map key events to `InputState` (UX-09); optional `deploy/studio-demo/native/` input bridge when A+B+C are closed on branch.
4. Blocked on: wgpu surface failure probe — **none** for mock IR merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-studio/src/lib.li` | `studio_err_gpu`, `StudioViewportErrorOverlay`, `studio_compose_viewport_error_overlay`, `studio_paint_viewport_error`, `studio_shell_viewport_cmds` + 6-arg `studio_shell_chrome_count_palette` | `studio_viewport_error.li` |
| `runtime/li_rt.c` | (already on branch) mock kind/retry | `li_rt_studio_viewport_error_*` |

## Not changed (scope fence)

- PH-GD-1 outliner — already on branch; **not** modified in this commit.
- `packages/li-world` — already on branch; **not** modified.
- SDL `InputState` ingest — **not** in this commit (see UX-09 release note).
- wgpu / asset loader failure detection — **not** wired.

## Breaking / Security / Performance / Downstream

- **Breaking:** N/A — completes documented UX-08 API on `StudioShellCompose`.
- **Security:** N/A — mock error kind only.
- **Performance:** N/A — O(1) overlay rects.
- **Downstream:** Hosts using 5-arg `studio_shell_chrome_count_palette` must pass `viewport_error_kind`.
