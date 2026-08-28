# Release notes: 2026-05-25 — ph-hw-wp2-li-render-else-fix

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** (feat/ph-hw-wp2-device-fix-render → feat/ph-hw-multi-vendor)  
**PH / REQ:** PH-HW WP2, PH-UX viewport_fps  
**Author:** agent

---

## Summary (one sentence)

`li-render` and dependent smokes pass `lic check` by replacing unsupported `else:` branches with sequential `if` blocks and float-only FPS accumulation (no `float()` cast).

## Agent continuation (required)

1. Read: `packages/li-render/src/lib.li`, `packages/li-scene/src/lib.li`, `packages/li-render/li-tests/smoke/viewport_fps_counter.li`
2. Run: `./build/compiler/lic/lic check packages/li-render/li-tests/smoke/viewport_fps_counter.li` and `lic check li-tests/composable/import_render_wgpu_fps.li` from `lic` repo root
3. Then: continue PH-HW WP3 `lig.present` integration on `feat/ph-hw-wp3-present` if host-present path is next
4. Blocked on: **none** for parse/check; parser `else` support still **not** implemented in `compiler/parser/parser.cpp`

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-render/src/lib.li` | Removed `else:`; default-then-`if` FPS tick; incremental float FPS; `raises Alloc` + `ensures` on HUD `str` procs | `lic check packages/li-render/src/lib.li` pass |
| `packages/li-scene/src/lib.li` | Same `else:` → `if` pattern for tier lookup and draw tick | transitive `lic check` for render→scene |
| `packages/li-math/src/lib.li` | `extern` math stubs: bounded `ensures` (not `ensures true` on `-> float`) | E0302 cleared in import graph |
| `packages/li-ui/src/lib.li` | `ui_frame_end`: `ensures frame.frame_id == old(frame.frame_id)` | E0302 cleared |
| `packages/li-render/li-tests/smoke/viewport_fps_counter.li` | Field reads instead of by-value helpers (E0311 move) | smoke pass |

## Not changed (scope fence)

- `compiler/parser/parser.cpp` — still no `KwElse` / `else_body` parsing
- `lig.present` / WP3 host-present path — **not** on `feat/ph-hw-multi-vendor` base
- LLVM / wgpu native pixel recording — **not** in this PR
- Benchmarks dashboard rows — **not** refreshed

## Breaking changes

None

## Security

N/A — grammar/typecheck-only; no trusted surface or CVE tests touched.

## Performance

N/A — FPS math equivalent for uniform `dt_ms` ticks; no bench CSV update.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / packages | N/A — in-tree `packages/li-render` only |

## CHANGELOG entry (paste into Unreleased)

```markdown
### Fixed
- **PH-HW WP2 li-render:** replace unsupported `else:` with sequential `if` + float FPS tick; `lic check` green on render smokes and `import_render_wgpu_fps.li` — [2026-05-25-ph-hw-wp2-li-render-else-fix.md](docs/release-notes/2026-05-25-ph-hw-wp2-li-render-else-fix.md).
```
