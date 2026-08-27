#!/usr/bin/env bash
# Build studio_shell_input_probe and emit one InputState JSON line (SDL or mock).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/studio_shell_input_probe.c"
BIN="${STUDIO_SHELL_INPUT_PROBE_BIN:-$ROOT/studio_shell_input_probe}"
WIDTH="${STUDIO_SHELL_INPUT_WIDTH:-1280}"
HEIGHT="${STUDIO_SHELL_INPUT_HEIGHT:-720}"
MOCK="${STUDIO_SHELL_INPUT_MOCK:-}"

build_probe() {
  if [[ -x "$BIN" ]]; then
    return 0
  fi
  if ! command -v pkg-config >/dev/null 2>&1; then
    echo "input-capture: pkg-config missing" >&2
    return 4
  fi
  if ! pkg-config --exists sdl2 2>/dev/null; then
    echo "input-capture: libsdl2-dev not installed" >&2
    return 4
  fi
  SDL_FLAGS="$(pkg-config --cflags --libs sdl2)"
  # shellcheck disable=SC2086
  gcc -std=c11 -Wall -Wextra -O2 "$SRC" -o "$BIN" $SDL_FLAGS
}

run_probe() {
  if [[ -n "$MOCK" ]]; then
    "$BIN" --mock "$MOCK"
    return
  fi
  if [[ "${STUDIO_SHELL_INPUT_MOCK_ONLY:-0}" == "1" ]]; then
    "$BIN" --mock "${STUDIO_SHELL_INPUT_MOCK_SPEC:-cmd_k,digit=3}"
    return
  fi
  local -a args=()
  [[ "${STUDIO_SHELL_KEY_ESCAPE:-0}" == "1" ]] && args+=(--key-escape)
  [[ "${STUDIO_SHELL_KEY_CMD_K:-0}" == "1" ]] && args+=(--key-cmd-k)
  if [[ -n "${STUDIO_SHELL_KEY_DIGIT:-}" ]]; then
    args+=(--key-digit "${STUDIO_SHELL_KEY_DIGIT}")
  fi
  if [[ ${#args[@]} -gt 0 ]]; then
    "$BIN" "${args[@]}"
    return
  fi
  "$BIN" --width "$WIDTH" --height "$HEIGHT"
}

build_probe || exit $?

if [[ -n "${DISPLAY:-}" ]]; then
  run_probe
elif command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a -s "-screen 0 ${WIDTH}x${HEIGHT}x24" bash -c "$(declare -f run_probe); run_probe"
elif command -v Xvfb >/dev/null 2>&1; then
  DISP=":198"
  Xvfb "$DISP" -screen 0 "${WIDTH}x${HEIGHT}x24" >/dev/null 2>&1 &
  XPID=$!
  sleep 0.5
  DISPLAY="$DISP" run_probe
  kill "$XPID" 2>/dev/null || true
else
  echo "input-capture: no DISPLAY; using mock cmd_k,digit=3" >&2
  "$BIN" --mock "${MOCK:-cmd_k,digit=3}"
fi
