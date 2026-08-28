#!/usr/bin/env bash
# Memory profiling smoke: native MD kernel, lic frontend, optional sanitizers.
#
# Threat model (summary):
#   - Li user code: no unsafe/Any; borrow + Lean gate on lic build.
#   - Trusted boundary: C reference kernels (md_core.c) — ASan on Linux CI.
#   - Parser/lexer: fuzz + malformed corpus must not crash (run_security.sh).
#
# Usage:
#   ./scripts/profile-memory.sh              # peak RSS + macOS leaks (if available)
#   ./scripts/profile-memory.sh --asan-md    # rebuild md with -fsanitize=address
#   ./scripts/profile-memory.sh --asan-lic   # rebuild lic with LI_SANITIZE=address
#
# Platform: macOS uses /usr/bin/time -l and leaks; Linux uses GNU time -v / valgrind.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MD_DIR="$ROOT/benchmarks/tier2_physics/md_lennard_jones"
BUILD_BENCH="$ROOT/build/bench/md_lennard_jones"
LIC="$ROOT/build/compiler/lic/lic"
NATIVE_BIN="$BUILD_BENCH/md_lj_native"
ASAN_MD=0
ASAN_LIC=0
ASAN_RT=0

for arg in "$@"; do
  case "$arg" in
    --asan-md) ASAN_MD=1 ;;
    --asan-lic) ASAN_LIC=1 ;;
    --asan-rt) ASAN_RT=1 ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

detect_llvm_dir() {
  if [[ -n "${LLVM_DIR:-}" && -d "$LLVM_DIR" ]]; then return 0; fi
  for d in /opt/homebrew/opt/llvm@22/lib/cmake/llvm \
           /usr/local/opt/llvm@22/lib/cmake/llvm \
           /usr/lib/llvm-22/lib/cmake/llvm; do
    if [[ -d "$d" ]]; then export LLVM_DIR="$d"; return 0; fi
  done
  echo "profile-memory: set LLVM_DIR" >&2
  return 1
}

build_native_md() {
  local cc="${CC:-clang}" extra=()
  if [[ "$ASAN_MD" -eq 1 ]]; then
    extra=(-fsanitize=address -fno-omit-frame-pointer -g)
  else
    extra=(-O2 -march=native)
  fi
  mkdir -p "$BUILD_BENCH"
  "$cc" "${extra[@]}" \
    "$MD_DIR/cpp/md_main.c" \
    "$MD_DIR/common/md_core.c" \
    "$MD_DIR/common/md_traj_env.c" \
    -lm -o "$NATIVE_BIN"
}

build_lic_asan() {
  detect_llvm_dir
  cmake -B "$ROOT/build-asan" -G Ninja -DLLVM_DIR="$LLVM_DIR" -DLI_SANITIZE=address "$ROOT"
  cmake --build "$ROOT/build-asan" --target lic
  LIC="$ROOT/build-asan/compiler/lic/lic"
}

build_rt_asan_smoke() {
  local cc="${CC:-clang}" out="$ROOT/build/bench/rt_asan_smoke"
  mkdir -p "$(dirname "$out")"
  local driver
  driver="$(mktemp -t li_rt_smoke.XXXXXX.c)"
  cat >"$driver" <<'EOF'
#include "li_rt.h"
int main(void) {
  li_rt_print_int(42);
  return 0;
}
EOF
  "$cc" -fsanitize=address -fno-omit-frame-pointer -g \
    -I"$ROOT/runtime" \
    "$driver" "$ROOT/runtime/li_rt.c" -o "$out"
  rm -f "$driver"
  "$out"
}

ensure_lic() {
  if [[ "$ASAN_LIC" -eq 1 ]]; then build_lic_asan; return; fi
  if [[ ! -x "$LIC" ]]; then detect_llvm_dir; "$ROOT/scripts/build.sh"; fi
}

peak_rss() {
  local label="$1"; shift
  echo "==> $label"
  if [[ "$(uname -s)" == "Darwin" ]] && [[ -x /usr/bin/time ]]; then
    /usr/bin/time -l "$@" 2>&1 | tee /dev/stderr | awk \
      '/maximum resident set size/ { printf "    peak RSS: %d bytes (%.2f MiB)\n", $1, $1/1048576 }'
  else
    local out
    out="$(/usr/bin/time -v "$@" 2>&1)" || true
    echo "$out" | awk '/Maximum resident set size/ { print "   ", $0 }'
  fi
}

md_traj_env_short() {
  export LI_MD_TRAJ="${TMPDIR:-/tmp}/li_md_traj_profile.txt"
  export LI_MD_TRAJ_STEPS="${LI_MD_TRAJ_STEPS:-80}"
  export LI_MD_TRAJ_STRIDE="${LI_MD_TRAJ_STRIDE:-10}"
  rm -f "$LI_MD_TRAJ"
}

run_macos_leaks() {
  command -v leaks >/dev/null 2>&1 || { echo "==> leaks: skip"; return 0; }
  echo "==> macOS leaks --atExit (md traj)"
  md_traj_env_short
  export LI_MD_TRAJ_STEPS=50
  leaks --atExit -- "$NATIVE_BIN" 2>&1 | tee /dev/stderr | grep -E 'leaks for|total leaked bytes' || true
}

run_valgrind_if_present() {
  command -v valgrind >/dev/null 2>&1 || { echo "==> valgrind: skip"; return 0; }
  echo "==> valgrind --leak-check=summary"
  md_traj_env_short
  export LI_MD_TRAJ_STEPS=40
  if ! valgrind --error-exitcode=42 --leak-check=summary -q "$NATIVE_BIN" 2>&1 | tail -8; then
    echo "==> valgrind: reported leaks (advisory — does not fail CI; fix or suppress in md_stress)"
    return 0
  fi
}

if [[ "$ASAN_RT" -eq 1 ]]; then
  echo "==> ASan li_rt smoke"
  build_rt_asan_smoke
fi

build_native_md
ensure_lic

SMALL="$ROOT/li-tests/lexer_parser/fib.li"
peak_rss "lic check fib.li" "$LIC" check "$SMALL"

md_traj_env_short
peak_rss "md_lj_native traj" "$NATIVE_BIN"

if [[ "$(uname -s)" == "Darwin" ]]; then
  run_macos_leaks
else
  run_valgrind_if_present
fi

chmod +x "$ROOT/li-tests/run_security.sh"
echo "==> security corpus"
"$ROOT/li-tests/run_security.sh"

echo "profile-memory: ok"
