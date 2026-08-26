#!/usr/bin/env bash
# One SDL keyboard poll (or mock) → InputState JSON + env for studio shell hosts.
# Usage (lic repo root):
#   ./scripts/studio-shell-sdl-tick.sh
#   STUDIO_SHELL_INPUT_MOCK=cmd_k,digit=3 ./scripts/studio-shell-sdl-tick.sh
#   STUDIO_SHELL_KEY_CMD_K=1 STUDIO_SHELL_KEY_DIGIT=3 ./scripts/studio-shell-sdl-tick.sh
#
# Exports: STUDIO_SHELL_INPUT_JSON, STUDIO_SHELL_KEY_*, STUDIO_SHELL_INPUT_MOCK
# Headless: prefers xvfb-run; without Xvfb uses mock (see deploy/studio-demo/native/input_capture.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE="$ROOT/deploy/studio-demo/native"
CAPTURE_SH="$NATIVE/input_capture.sh"
META="${STUDIO_SHELL_INPUT_META:-$ROOT/data/studio-ui-ux-plan-loop/latest-shell-input.json}"

if [[ ! -f "$CAPTURE_SH" ]]; then
  echo "studio-shell-sdl-tick: missing $CAPTURE_SH" >&2
  exit 2
fi
chmod +x "$CAPTURE_SH" 2>/dev/null || true

# Simulate keys without SDL when explicitly requested (CI / macOS without xvfb).
if [[ "${STUDIO_SHELL_FORCE_MOCK:-0}" == "1" ]]; then
  export STUDIO_SHELL_INPUT_MOCK_ONLY=1
  export STUDIO_SHELL_INPUT_MOCK_SPEC="${STUDIO_SHELL_INPUT_MOCK:-cmd_k,digit=3}"
fi

emit_mock_json_python() {
  python3 - <<'PY'
import json, os
spec = os.environ.get("STUDIO_SHELL_INPUT_MOCK", "") or os.environ.get(
    "STUDIO_SHELL_INPUT_MOCK_SPEC", "cmd_k,digit=3"
)
state = {
    "pointer_down": 0,
    "pointer_x": 0.0,
    "pointer_y": 0.0,
    "key_escape": 0,
    "key_cmd_k": 0,
    "key_digit": 0,
    "mock": True,
    "capture_mode": "script_mock",
}
for tok in spec.replace(" ", "").split(","):
    if tok in ("escape", "key_escape"):
        state["key_escape"] = 1
    elif tok in ("cmd_k", "key_cmd_k"):
        state["key_cmd_k"] = 1
    elif tok.startswith("digit=") or tok.startswith("key_digit="):
        d = int(tok.split("=", 1)[1])
        if 1 <= d <= 5:
            state["key_digit"] = d
if os.environ.get("STUDIO_SHELL_KEY_ESCAPE") == "1":
    state["key_escape"] = 1
if os.environ.get("STUDIO_SHELL_KEY_CMD_K") == "1":
    state["key_cmd_k"] = 1
if os.environ.get("STUDIO_SHELL_KEY_DIGIT", "").isdigit():
    state["key_digit"] = int(os.environ["STUDIO_SHELL_KEY_DIGIT"])
print(json.dumps(state))
PY
}

json_line=""
if [[ "${STUDIO_SHELL_INPUT_MOCK_ONLY:-0}" == "1" || -n "${STUDIO_SHELL_INPUT_MOCK:-}" || "${STUDIO_SHELL_FORCE_MOCK:-0}" == "1" ]]; then
  json_line="$(emit_mock_json_python)"
else
  json_line="$(
    STUDIO_SHELL_INPUT_WIDTH="${STUDIO_SHELL_INPUT_WIDTH:-1280}" \
    STUDIO_SHELL_INPUT_HEIGHT="${STUDIO_SHELL_INPUT_HEIGHT:-720}" \
    STUDIO_SHELL_INPUT_MOCK="${STUDIO_SHELL_INPUT_MOCK:-}" \
    STUDIO_SHELL_INPUT_MOCK_ONLY="${STUDIO_SHELL_INPUT_MOCK_ONLY:-0}" \
    STUDIO_SHELL_INPUT_MOCK_SPEC="${STUDIO_SHELL_INPUT_MOCK_SPEC:-cmd_k,digit=3}" \
    STUDIO_SHELL_KEY_ESCAPE="${STUDIO_SHELL_KEY_ESCAPE:-0}" \
    STUDIO_SHELL_KEY_CMD_K="${STUDIO_SHELL_KEY_CMD_K:-0}" \
    STUDIO_SHELL_KEY_DIGIT="${STUDIO_SHELL_KEY_DIGIT:-}" \
    bash "$CAPTURE_SH" 2>/dev/null | tail -n 1
  )" || json_line=""
fi

if [[ -z "$json_line" || "$json_line" != \{* ]]; then
  if [[ -n "${STUDIO_SHELL_INPUT_MOCK:-}" || "${STUDIO_SHELL_FORCE_MOCK:-0}" == "1" ]]; then
    json_line="$(emit_mock_json_python)"
  else
    echo "studio-shell-sdl-tick: probe failed (install libsdl2-dev or STUDIO_SHELL_FORCE_MOCK=1)" >&2
    exit 4
  fi
fi

if [[ -z "$json_line" || "$json_line" != \{* ]]; then
  echo "studio-shell-sdl-tick: expected JSON line from probe" >&2
  exit 5
fi

export STUDIO_SHELL_INPUT_JSON="$json_line"

eval "$(
  python3 - <<'PY'
import json, os, shlex, sys
data = json.loads(os.environ["STUDIO_SHELL_INPUT_JSON"])
fields = {
    "STUDIO_SHELL_POINTER_DOWN": str(int(data.get("pointer_down", 0))),
    "STUDIO_SHELL_POINTER_X": str(float(data.get("pointer_x", 0.0))),
    "STUDIO_SHELL_POINTER_Y": str(float(data.get("pointer_y", 0.0))),
    "STUDIO_SHELL_KEY_ESCAPE": str(int(data.get("key_escape", 0))),
    "STUDIO_SHELL_KEY_CMD_K": str(int(data.get("key_cmd_k", 0))),
    "STUDIO_SHELL_KEY_DIGIT": str(int(data.get("key_digit", 0))),
    "STUDIO_SHELL_INPUT_MOCK": "1" if data.get("mock") else "0",
}
for k, v in fields.items():
    print(f"export {k}={shlex.quote(v)}")
PY
)"

mkdir -p "$(dirname "$META")"
python3 - "$META" <<'PY'
import json, os, sys
from pathlib import Path
meta = Path(sys.argv[1])
payload = json.loads(os.environ["STUDIO_SHELL_INPUT_JSON"])
meta.write_text(
    json.dumps(
        {
            "status": "pass",
            "input": payload,
            "note": "SDL/mock probe — map to InputState then studio_handle_studio_key each frame",
        },
        indent=2,
    )
    + "\n",
    encoding="utf-8",
)
PY

echo "$STUDIO_SHELL_INPUT_JSON"
printf 'studio-shell-sdl-tick: escape=%s cmd_k=%s digit=%s mock=%s\n' \
  "$STUDIO_SHELL_KEY_ESCAPE" "$STUDIO_SHELL_KEY_CMD_K" "$STUDIO_SHELL_KEY_DIGIT" \
  "$STUDIO_SHELL_INPUT_MOCK" >&2
