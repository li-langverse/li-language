#!/usr/bin/env bash
# Mirror GitHub Actions CI locally before opening/updating a PR.
#
# Usage:
#   ./scripts/local-ci.sh              # native (macOS or Linux)
#   ./scripts/local-ci.sh --memory       # after ci.sh: leak/RSS + security corpus
#   ./scripts/local-ci.sh --docker     # prebuilt GHCR image (ubuntu24 + LLVM 22)
#   ./scripts/local-ci.sh --prepare-docker  # pull/build image only, then exit
#
# Env: LI_CI_DOCKER_IMAGE (default ghcr.io/li-langverse/lic-ci:debian12-llvm22)
# See docs/ecosystem/local-ci-docker-images.md
#
# Exits non-zero on the same failures as scripts/ci.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
USE_DOCKER=0
RUN_MEMORY=0
PREPARE_DOCKER=0
for arg in "$@"; do
  case "$arg" in
    --docker) USE_DOCKER=1 ;;
    --memory) RUN_MEMORY=1 ;;
    --prepare-docker) PREPARE_DOCKER=1 ;;
    -h|--help)
      sed -n '2,14p' "$0"
      exit 0
      ;;
    *)
      echo "unknown arg: $arg (try --docker, --prepare-docker, --memory, or --help)" >&2
      exit 2
      ;;
  esac
done

LI_CI_DOCKER_IMAGE="${LI_CI_DOCKER_IMAGE:-ghcr.io/li-langverse/lic-ci:debian12-llvm22}"

# shellcheck source=lib/container-runtime.sh
source "$ROOT/scripts/lib/container-runtime.sh"

require_container_runtime() {
  CTR="$(resolve_container_runtime)" || {
    echo "local-ci: need podman or docker (podman preferred when available)" >&2
    exit 1
  }
  export CTR
}

run_docker_ci() {
  require_container_runtime
  chmod +x "$ROOT/scripts/prepare-docker-ci-image.sh"
  CONTAINER_RUNTIME="$CTR" LI_CI_DOCKER_IMAGE="$LI_CI_DOCKER_IMAGE" "$ROOT/scripts/prepare-docker-ci-image.sh"

  local stage="/tmp/li-local-ci-$$"
  # shellcheck disable=SC2064
  trap "rm -rf '$stage'" EXIT
  if command -v rsync >/dev/null 2>&1; then
    rsync -a \
      --exclude build \
      --exclude .git \
      --exclude .venv-plot \
      --exclude benchmarks/results \
      "$ROOT/" "$stage/"
  else
    mkdir -p "$stage"
    cp -a "$ROOT/." "$stage/"
    rm -rf "$stage/build" "$stage/.git" "$stage/.venv-plot" "$stage/benchmarks/results"
  fi
  echo "==> $CTR run $LI_CI_DOCKER_IMAGE"
  "$CTR" run --rm \
    -e LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm \
    -e CC=clang-22 \
    -e CXX=clang++-22 \
    -e LI_REPO_ROOT=/src \
    -e HTTPD_SKIP_LI_ROUTING_BIN="${HTTPD_SKIP_LI_ROUTING_BIN:-0}" \
    -e HTTPD_SKIP_AUTH_BEARER_SMOKE=1 \
    -v "$stage:/src" \
    -w /src \
    "$LI_CI_DOCKER_IMAGE" \
    bash -lc 'chmod +x scripts/ci.sh scripts/build.sh scripts/local-ci.sh; ./scripts/ci.sh'
}

detect_llvm_dir() {
  if [[ -n "${LLVM_DIR:-}" && -d "$LLVM_DIR" ]]; then
    return 0
  fi
  local candidates=(
    /opt/homebrew/opt/llvm@22/lib/cmake/llvm
    /usr/local/opt/llvm@22/lib/cmake/llvm
    /usr/lib/llvm-22/lib/cmake/llvm
  )
  for d in "${candidates[@]}"; do
    if [[ -d "$d" ]]; then
      export LLVM_DIR="$d"
      return 0
    fi
  done
  echo "local-ci: set LLVM_DIR to LLVM 22 CMake package" >&2
  echo "  macOS: export LLVM_DIR=\$(brew --prefix llvm@22)/lib/cmake/llvm" >&2
  echo "  Ubuntu: export LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm" >&2
  return 1
}

detect_compilers() {
  if [[ -z "${CC:-}" ]]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
      # Homebrew llvm@22 clang++ mixes libc++ with Xcode SDK headers badly.
      export CC=clang
      export CXX=clang++
    elif command -v clang-22 >/dev/null 2>&1; then
      export CC=clang-22
      export CXX=clang++-22
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
      echo "  sudo apt install cmake ninja-build clang-22 llvm-22-dev zlib1g-dev libzstd-dev python3" >&2
    else
      echo "  brew install llvm@22 cmake ninja python3" >&2
    fi
    return 1
  fi
  detect_llvm_dir
}

if [[ "$PREPARE_DOCKER" -eq 1 ]]; then
  require_container_runtime
  chmod +x "$ROOT/scripts/prepare-docker-ci-image.sh"
  CONTAINER_RUNTIME="$CTR" LI_CI_DOCKER_IMAGE="$LI_CI_DOCKER_IMAGE" "$ROOT/scripts/prepare-docker-ci-image.sh"
  echo "local-ci: image ready ($LI_CI_DOCKER_IMAGE)"
  exit 0
fi

if [[ "$USE_DOCKER" -eq 1 ]]; then
  require_container_runtime
  echo "==> local-ci ($CTR / $LI_CI_DOCKER_IMAGE)"
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
if [[ "$RUN_MEMORY" -eq 1 ]]; then
  chmod +x "$ROOT/scripts/memory-ci.sh"
  "$ROOT/scripts/memory-ci.sh"
fi
echo "local-ci: ok (native)"
