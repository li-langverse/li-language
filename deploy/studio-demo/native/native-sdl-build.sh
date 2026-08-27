#!/usr/bin/env bash
# Build a native stub binary (SDL when needed; CPU-only sources need no SDL).
set -euo pipefail
SRC="${1:?source.c}"
OUT="${2:?output binary}"
if [[ -x "$OUT" ]]; then
  exit 0
fi
needs_sdl=1
if grep -q 'SDL.h' "$SRC" 2>/dev/null; then
  needs_sdl=1
else
  needs_sdl=0
fi
if [[ "$needs_sdl" -eq 0 ]]; then
  cc -std=c11 -Wall -Wextra -O2 "$SRC" -o "$OUT"
  exit 0
fi
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
  # shellcheck disable=SC2046
  gcc -std=c11 -Wall -Wextra -O2 "$SRC" -o "$OUT" $(pkg-config --cflags --libs sdl2)
  exit 0
fi
if command -v sdl2-config >/dev/null 2>&1; then
  # shellcheck disable=SC2046
  cc -std=c11 -Wall -Wextra -O2 "$SRC" -o "$OUT" $(sdl2-config --cflags --libs)
  exit 0
fi
echo "native-sdl-build: install SDL2 (libsdl2-dev or brew install sdl2)" >&2
exit 4
