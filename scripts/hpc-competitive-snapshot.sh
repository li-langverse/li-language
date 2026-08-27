#!/usr/bin/env bash
# Record toolchain versions for competitive review (optional env pins).
# No network calls — reads local commands and env only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

OUT="${LI_HPC_COMPETITIVE_SNAPSHOT:-$ROOT/benchmarks/competitive/snapshots/latest.env}"
mkdir -p "$(dirname "$OUT")"

li_phase "HPC competitive snapshot"

{
  echo "# generated $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "LI_REPO_SHA=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "LI_REGISTRY=$(git -C "$ROOT" log -1 --format=%h -- benchmarks/competitive/registry.toml 2>/dev/null || echo unknown)"

  ver() {
    local name="$1" cmd="$2"
    if command -v "$cmd" &>/dev/null; then
      echo "${name}=$("$cmd" 2>/dev/null | head -1 | tr -d '\r')"
    else
      echo "${name}=missing"
    fi
  }

  ver HPC_CLANG clang
  ver HPC_RUSTC rustc
  ver HPC_CARGO cargo
  ver HPC_JULIA julia
  ver HPC_PYTHON3 python3

  # Optional explicit pins (set in CI or local review shell)
  for v in \
    HPC_COMPETITIVE_CPP_SHA \
    HPC_COMPETITIVE_RUST_SHA \
    HPC_COMPETITIVE_JULIA_VERSION \
    HPC_COMPETITIVE_CHAPEL_VERSION \
    HPC_COMPETITIVE_KOKKOS_VERSION; do
    if [[ -n "${!v:-}" ]]; then
      echo "${v}=${!v}"
    fi
  done
} >"$OUT"

li_ok "wrote $OUT"
cat "$OUT"
