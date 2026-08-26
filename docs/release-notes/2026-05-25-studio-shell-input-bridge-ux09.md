# Release notes: 2026-05-25 — studio-shell-input-bridge-ux09

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-UX, UX-09  
**Author:** agent

---

## Summary (one sentence)

SDL/mock keyboard ingest probe emits `InputState` JSON; `studio-shell-sdl-tick.sh` exports host env; bridge doc + `studio_keyboard_input_json.li` close UX-09 to score **3**.

## Agent continuation (required)

1. Read: `docs/game-dev/studio-shell-input-bridge.md`, `deploy/studio-demo/native/studio_shell_input_probe.c`.
2. Run: `chmod +x scripts/studio-shell-sdl-tick.sh deploy/studio-demo/native/input_capture.sh`; `STUDIO_SHELL_FORCE_MOCK=1 ./scripts/studio-shell-sdl-tick.sh`; `lic check packages/li-gui/li-tests/smoke/studio_keyboard_input_json.li`.
3. Then: wire real host loop (poll SDL each frame → `studio_handle_studio_key` before paint); refresh `data/studio-ui-ux-plan-loop/latest-ux-assessment.json` UX-09 → **3.0**.
4. Blocked on: full `li-studio` window binary — **none** for ingest stub merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `deploy/studio-demo/native/` | `studio_shell_input_probe.c`, `input_capture.sh` | mock JSON line; SDL when Xvfb available |
| `scripts/studio-shell-sdl-tick.sh` | env `STUDIO_SHELL_INPUT_JSON`, `STUDIO_SHELL_KEY_*` | script stdout |
| `docs/game-dev/studio-shell-input-bridge.md` | SDL → `InputState` → `studio_handle_studio_key` | UX-09 rubric link |
| `packages/li-gui` | smoke `studio_keyboard_input_json.li` | manifest row |
| `deploy/studio-demo/README.md` | UX-09 native ingest section | — |

## Not changed (scope fence)

- Full li-studio SDL/wgpu window — **not** in this commit.
- Fuzzy command palette search (UX-04) — **not** touched.
- `li-ui` `InputState` field set — **unchanged** (probe matches existing fields).

## Breaking / Security / Performance / Downstream

- **Breaking:** N/A — additive scripts and native stub.
- **Security:** N/A — probe runs locally; no network.
- **Performance:** N/A — one SDL poll per tick script invocation.
- **Downstream:** Hosts should read `STUDIO_SHELL_INPUT_JSON` or map env vars each frame.
