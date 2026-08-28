# Studio shell input bridge (SDL → `InputState`)

**Audience:** native hosts wiring `li-studio` shell demo / future `li-gui` window.  
**Rubric:** [UX-09 Keyboard-first workflows](competitive-intel/ui-ux-by-dimension.md) (score **3** = instrumented global shortcuts like Blender / Linear).

## Contract

`li-ui` defines `InputState` (`packages/li-ui/src/lib.li`):

| Field | Type | Host duty |
|-------|------|-----------|
| `pointer_down` | `int` | 0/1 left button |
| `pointer_x`, `pointer_y` | `float` | client coords |
| `key_escape` | `int` | 0/1 Escape pressed this frame |
| `key_cmd_k` | `int` | 0/1 Cmd+K or Ctrl+K |
| `key_digit` | `int` | 0 none; 1–5 region focus digits |

Each frame the host:

1. Polls SDL (or injects mock keys in CI).
2. Fills `InputState` (or parses probe JSON into those fields).
3. Calls `studio_handle_studio_key(compose, input)` **before** paint when shortcuts should affect compose (palette open, region focus).
4. Runs `studio_paint_shell_chrome` / present path with updated compose (`studio_shell_host_frame` → `lig.present` when `LIG_HOST_PRESENT=1`; see [2026-05-25-lig-present-hw1.md](../release-notes/2026-05-25-lig-present-hw1.md)).

```text
SDL_PollEvent / GetKeyboardState
        ↓
  InputState (Li)
        ↓
studio_handle_studio_key → gui_handle_studio_key_palette
        ↓
  compose.palette / compose.panel updated
        ↓
studio_paint_shell_chrome(frame, compose)
```

## Native probe (honest ingest, no full window)

| Artifact | Role |
|----------|------|
| `deploy/studio-demo/native/studio_shell_input_probe.c` | One tick: SDL keyboard + mouse **or** `--mock cmd_k,digit=3` |
| `deploy/studio-demo/native/input_capture.sh` | Build probe; Xvfb when `DISPLAY` unset |
| `scripts/studio-shell-sdl-tick.sh` | CI/host entry: exports `STUDIO_SHELL_INPUT_JSON`, `STUDIO_SHELL_KEY_*` |
| `deploy/studio-demo/native/studio_shell_present_host.c` | One SDL present tick (`LIG_HOST_PRESENT=1`) for PH-HW WP3 |
| `scripts/studio-shell-present-tick.sh` | Build/run present host or mock JSON |

### Run

```bash
# From lic repo root (libsdl2-dev; xvfb-run on headless Linux)
./scripts/studio-shell-sdl-tick.sh

# Mock path (no DISPLAY / force CI keys)
STUDIO_SHELL_INPUT_MOCK=cmd_k,digit=3 ./scripts/studio-shell-sdl-tick.sh
STUDIO_SHELL_FORCE_MOCK=1 ./scripts/studio-shell-sdl-tick.sh
```

Example stdout / env JSON:

```json
{"pointer_down":0,"pointer_x":0.0,"pointer_y":0.0,"key_escape":0,"key_cmd_k":1,"key_digit":3,"mock":true,"capture_mode":"argv_mock"}
```

Metadata: `data/studio-ui-ux-plan-loop/latest-shell-input.json`.

### SDL mapping (probe implementation)

| SDL | `InputState` |
|-----|----------------|
| `SDL_SCANCODE_ESCAPE` | `key_escape = 1` |
| `(KMOD_GUI \| KMOD_CTRL) + SDL_SCANCODE_K` | `key_cmd_k = 1` |
| `SDL_SCANCODE_1` … `_5` | `key_digit = 1` … `5` |
| mouse left + position | `pointer_down`, `pointer_x`, `pointer_y` |

## Headless Li demo (today)

`packages/li-studio/src/main.li` sets `input.key_cmd_k = 1` on frame 2 without SDL — proves `studio_handle_studio_key` in the compose path. The probe + this doc are the **host bridge** to score UX-09 **3/3** in the plan loop (instrumented native key map, not HTML hints alone).

## Tests

| Test | Evidence |
|------|----------|
| `packages/li-gui/li-tests/smoke/studio_keyboard.li` | IR: Escape / Cmd+K / digit → `gui_handle_studio_key` |
| `packages/li-gui/li-tests/smoke/studio_keyboard_input_json.li` | Fixed mock matching probe JSON → palette + region 3 |
| `packages/li-studio/li-tests/smoke/studio_shell_demo.li` | Per-frame `studio_handle_studio_key` in shell demo |

## UX-09 scoring (plan loop)

| Score | Bar | This bridge |
|-------|-----|-------------|
| 1–2 | HTML hints / hooks only | Prior iterations |
| **3** | Global shortcuts **instrumented** in native path | Probe JSON + env + per-frame `studio_handle_studio_key` doc |

**Estimate after merge:** UX-09 **3.0** — native `InputState` ingest stub, mock + SDL paths, linked rubric and smokes. Full fuzzy palette (UX-04) remains separate.

## Related

- Viewport pixels (UX-14): `deploy/studio-demo/native/studio_viewport_capture.c`, `scripts/studio-ui-ux-capture-native.sh`
- Keyboard IR release note: `docs/release-notes/2026-05-25-studio-keyboard-ux09.md`
- PH-GD-1 shell demo: `packages/li-studio/examples/studio_shell_demo.toml`
