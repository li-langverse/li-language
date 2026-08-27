# Release notes: 2026-05-25 — studio-keyboard-ux09

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-UX, UX-09  
**Author:** agent

---

## Summary (one sentence)

Studio keyboard-first IR: Escape / Cmd+K / digits 1–5 wired through `gui_handle_studio_key` with palette compose path and smoke tests.

## Agent continuation (required)

1. Read: `packages/li-gui/README.md`, `packages/li-gui/src/lib.li` (UX-09 section), `docs/game-dev/competitive-intel/ui-ux-by-dimension.md` (UX-09).
2. Run: `lic check packages/li-gui/src/lib.li`; `lic check packages/li-gui/li-tests/smoke/studio_keyboard.li`; monorepo composable `import_gui_studio_keyboard.li` when `run_all.sh` has lic on PATH.
3. Then: SDL/native host should set `InputState.key_cmd_k`, `key_digit`, `key_escape` from real key events; score UX-09 ≥ 2.5 in plan loop assessment.
4. Blocked on: native key ingest — **none** for IR merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-ui` | `InputState.key_cmd_k`, `key_digit`; `input_default`; palette move-safety in `studio_compose_palette` / `studio_palette_open` | `lic check packages/li-ui/src/lib.li` |
| `packages/li-gui` | `StudioKeyBinding`, `gui_handle_studio_key`, palette compose helpers | `packages/li-gui/li-tests/smoke/studio_keyboard.li` |
| `packages/li-studio` | `studio_handle_studio_key` → `gui_handle_studio_key_palette` | `import studio` composable tests |
| `li-tests` | `composable/import_gui_studio_keyboard.li` | manifest row |

## Not changed (scope fence)

- Fuzzy command palette search runtime (UX-04) — **not** in this PR.
- SDL / Xvfb key event wiring — **not** in this PR.
- LLVM / httpd / tier5 — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** N/A — additive `InputState` fields default to 0.
- **Security:** N/A — no trusted surface change.
- **Performance:** N/A — O(1) key dispatch.
- **Downstream:** Hosts must populate new `InputState` fields when enabling keyboard shortcuts.
