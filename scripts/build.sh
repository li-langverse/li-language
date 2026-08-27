#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"

if ! li_detect_llvm_dir; then
  li_llvm_install_hint
  exit 1
fi
li_detect_compilers
export CC CXX LLVM_DIR LI_LLVM_MAJOR

# Parallel Ninja builds: override with LI_BUILD_JOBS=1 for deterministic logs.
li_build_jobs() {
  if [[ -n "${LI_BUILD_JOBS:-}" ]]; then
    echo "$LI_BUILD_JOBS"
    return
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null || echo 4
  elif command -v nproc >/dev/null 2>&1; then
    nproc
  else
    echo 4
  fi
}

cmake -B "$ROOT/build" -G Ninja -DLLVM_DIR="$LLVM_DIR" "$@"
cmake --build "$ROOT/build" -j "$(li_build_jobs)" "$@"
