#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$ROOT/build/compiler/lic/lic}"
OUT="${1:-$ROOT/examples/tetris/tetris}"

export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}:/opt/homebrew/lib/pkgconfig"

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config required" >&2
  exit 1
fi

SDL_FLAGS="$(pkg-config --cflags --libs sdl2)"
export LI_EXTRA_C="$ROOT/examples/tetris/tetris_rt.c"

"$LIC" build "$ROOT/examples/tetris/main.li" -o "$OUT" --release $SDL_FLAGS
echo "built $OUT"
