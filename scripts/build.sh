#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_DIR="${LLVM_DIR:-}"
if [[ -z "$LLVM_DIR" && -d /opt/homebrew/opt/llvm@18/lib/cmake/llvm ]]; then
  LLVM_DIR=/opt/homebrew/opt/llvm@18/lib/cmake/llvm
fi
if [[ -z "$LLVM_DIR" && -d /usr/lib/llvm-18/lib/cmake/llvm ]]; then
  LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
fi
if [[ -z "$LLVM_DIR" ]]; then
  echo "Set LLVM_DIR to LLVM 18 CMake package (e.g. \$(brew --prefix llvm@18)/lib/cmake/llvm)" >&2
  exit 1
fi
cmake -B "$ROOT/build" -G Ninja -DLLVM_DIR="$LLVM_DIR" "$@"
cmake --build "$ROOT/build" "$@"
