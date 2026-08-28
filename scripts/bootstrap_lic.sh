#!/usr/bin/env bash
# Build the Li bootstrap compiler binary from Li source (C++ host compiles it).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$ROOT/build/compiler/lic/lic}"
OUT="${1:-$ROOT/build/lic-from-li}"

if [[ ! -x "$LIC" ]]; then
  echo "bootstrap: build C++ lic first (./scripts/build.sh)" >&2
  exit 1
fi

"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$OUT" --release
echo "bootstrap: built $OUT"
"$OUT" --version
"$OUT" smoke
echo "bootstrap: smoke ok"
