#!/usr/bin/env bash
# Mirror GitHub Actions CI locally before opening/updating a PR.
#
# Usage:
#   ./scripts/local-ci.sh              # native (macOS or Linux)
#   ./scripts/local-ci.sh --docker     # Ubuntu 24.04 container (closest to GHA linux job)
#
# Exits non-zero on the same failures as scripts/ci.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
USE_DOCKER=0
for arg in "$@"; do
  case "$arg" in
    --docker) USE_DOCKER=1 ;;
    -h|--help)
      sed -n '2,10p' "$0"
      exit 0
      ;;
    *)
      echo "unknown arg: $arg (try --docker or --help)" >&2
      exit 2
      ;;
  esac
done

run_docker_ci() {
  local stage="/tmp/li-local-ci-$$"
  # shellcheck disable=SC2064
  trap "rm -rf '$stage'" EXIT
  rsync -a \
    --exclude build \
    --exclude .git \
    --exclude .venv-plot \
    --exclude benchmarks/results \
    "$ROOT/" "$stage/"
  docker run --rm -v "$stage:/src" -w /src ubuntu:24.04 bash -lc '
    set -euo pipefail
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq cmake ninja-build clang-18 llvm-18-dev zlib1g-dev libzstd-dev python3 rsync
    export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
    export CC=clang-18
    export CXX=clang++-18
    chmod +x scripts/ci.sh scripts/build.sh scripts/local-ci.sh
    ./scripts/ci.sh
  '
}

detect_llvm_dir() {
  if [[ -n "${LLVM_DIR:-}" && -d "$LLVM_DIR" ]]; then
    return 0
  fi
  local candidates=(
    /opt/homebrew/opt/llvm@18/lib/cmake/llvm
    /usr/local/opt/llvm@18/lib/cmake/llvm
    /usr/lib/llvm-18/lib/cmake/llvm
  )
  for d in "${candidates[@]}"; do
    if [[ -d "$d" ]]; then
      export LLVM_DIR="$d"
      return 0
    fi
  done
  echo "local-ci: set LLVM_DIR to LLVM 18 CMake package" >&2
  echo "  macOS: export LLVM_DIR=\$(brew --prefix llvm@18)/lib/cmake/llvm" >&2
  echo "  Ubuntu: export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm" >&2
  return 1
}

detect_compilers() {
  if [[ -z "${CC:-}" ]]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
      # Homebrew llvm@18 clang++ mixes libc++ with Xcode SDK headers badly.
      export CC=clang
      export CXX=clang++
    elif command -v clang-18 >/dev/null 2>&1; then
      export CC=clang-18
      export CXX=clang++-18
    else
      export CC=clang
      export CXX=clang++
    fi
  fi
}

check_native_prereqs() {
  local missing=()
  for cmd in cmake ninja python3 "$CC"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      missing+=("$cmd")
    fi
  done
  if ((${#missing[@]} > 0)); then
    echo "local-ci: missing tools: ${missing[*]}" >&2
    if [[ "$(uname -s)" == "Linux" ]]; then
      echo "  sudo apt install cmake ninja-build clang-18 llvm-18-dev zlib1g-dev libzstd-dev python3" >&2
    else
      echo "  brew install llvm@18 cmake ninja python3" >&2
    fi
    return 1
  fi
  detect_llvm_dir
}

if [[ "$USE_DOCKER" -eq 1 ]]; then
  if ! command -v docker >/dev/null 2>&1; then
    echo "local-ci: docker not found" >&2
    exit 1
  fi
  echo "==> local-ci (docker / ubuntu-24.04)"
  run_docker_ci
  echo "local-ci: ok (docker)"
  exit 0
fi

echo "==> local-ci (native)"
detect_compilers
check_native_prereqs
chmod +x "$ROOT/scripts/ci.sh" "$ROOT/scripts/build.sh" "$ROOT/scripts/check-version.sh"
"$ROOT/scripts/check-version.sh"
"$ROOT/scripts/ci.sh"
"$ROOT/scripts/check-version.sh" --build
echo "local-ci: ok (native)"
