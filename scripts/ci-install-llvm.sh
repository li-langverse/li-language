#!/usr/bin/env bash
# Install pinned LLVM for CI / devbox (Ubuntu/Debian). Idempotent-ish.
# Usage: sudo bash scripts/ci-install-llvm.sh
# Env: LI_LLVM_MAJOR (default 22)
set -euo pipefail
VER="${LI_LLVM_MAJOR:-22}"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq \
  cmake ninja-build wget gnupg ca-certificates \
  zlib1g-dev libzstd-dev python3 pkg-config
if ! apt-get install -y -qq "clang-${VER}" "llvm-${VER}-dev" "lld-${VER}" 2>/dev/null; then
  wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
  chmod +x /tmp/llvm.sh
  /tmp/llvm.sh "$VER"
  apt-get install -y -qq "clang-${VER}" "llvm-${VER}-dev" "lld-${VER}"
fi
echo "LLVM ${VER} at /usr/lib/llvm-${VER}/lib/cmake/llvm"
