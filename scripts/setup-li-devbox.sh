#!/usr/bin/env bash
# One-time Li development setup for Debian 12+ dev boxes (e.g. engine).
# Run: sudo bash scripts/setup-li-devbox.sh
# Then as user: bash scripts/setup-li-devbox.sh --user
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NODE_VERSION="${LI_DEVBOX_NODE_VERSION:-24.16.0}"
NODE_PREFIX="${LI_DEVBOX_NODE_PREFIX:-/home/s4il0r/.local/node}"
ENV_SNIPPET="/etc/profile.d/li-dev.sh"
USER_ENV="${HOME}/.config/environment.d/99-li-dev.conf"

usage() {
  echo "Usage:"
  echo "  sudo bash $0              # apt packages + system LLVM profile"
  echo "  bash $0 --user            # user Node.js + ~/.config/environment.d (no sudo)"
}

install_node_user() {
  if [[ -x "${NODE_PREFIX}/bin/node" ]]; then
    echo "==> Node already at ${NODE_PREFIX}/bin/node"
    return
  fi
  echo "==> Installing Node ${NODE_VERSION} to ${NODE_PREFIX}"
  mkdir -p "$(dirname "$NODE_PREFIX")"
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' EXIT
  arch="$(uname -m)"
  case "$arch" in
    x86_64) node_arch=linux-x64 ;;
    aarch64) node_arch=linux-arm64 ;;
    *) echo "unsupported arch: $arch" >&2; exit 1 ;;
  esac
  tarball="node-v${NODE_VERSION}-${node_arch}.tar.xz"
  curl -fsSL "https://nodejs.org/dist/v${NODE_VERSION}/${tarball}" -o "${tmp}/${tarball}"
  tar -xJf "${tmp}/${tarball}" -C "${tmp}"
  rm -rf "$NODE_PREFIX"
  mv "${tmp}/node-v${NODE_VERSION}-${node_arch}" "$NODE_PREFIX"
  echo "    ${NODE_PREFIX}/bin/node $("${NODE_PREFIX}/bin/node" -v)"
}

write_user_env() {
  mkdir -p "$(dirname "$USER_ENV")"
  cat >"$USER_ENV" <<EOF
# Li devbox — sourced by systemd user sessions (see docs/guide/devbox-li-development.md)
PATH=${NODE_PREFIX}/bin:\$PATH
LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
CC=clang-18
CXX=clang++-18
LI_REPO_ROOT=${ROOT}
LIC_ROOT=${ROOT}
EOF
  echo "==> wrote $USER_ENV"
}

install_system() {
  if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    echo "Run as root: sudo bash $0" >&2
    exit 1
  fi
  export DEBIAN_FRONTEND=noninteractive
  echo "==> apt: build toolchain + LLVM 18"
  apt-get update -qq
  apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    ca-certificates \
    python3 \
    python3-venv \
    pkg-config \
    clang-18 \
    llvm-18-dev \
    lld-18

  cat >"$ENV_SNIPPET" <<'EOF'
# Li compiler — LLVM 18 (Debian bookworm)
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export CC=clang-18
export CXX=clang++-18
EOF
  chmod 644 "$ENV_SNIPPET"
  echo "==> wrote $ENV_SNIPPET"
}

build_lic() {
  echo "==> build lic (cmake + ninja)"
  # shellcheck source=/dev/null
  source "$ENV_SNIPPET" 2>/dev/null || true
  export LLVM_DIR="${LLVM_DIR:-/usr/lib/llvm-18/lib/cmake/llvm}"
  export CC="${CC:-clang-18}"
  export CXX="${CXX:-clang++-18}"
  "$ROOT/scripts/build.sh"
  echo "==> lic binary: $ROOT/build/compiler/lic/lic"
  "$ROOT/build/compiler/lic/lic" --version 2>/dev/null || true
}

user_setup() {
  install_node_user
  write_user_env
  echo ""
  echo "User setup done. Open a new shell or: source $USER_ENV"
  echo "Then: cd $ROOT && ./scripts/build.sh && ./scripts/httpd-plan-gates.sh"
}

main() {
  case "${1:-}" in
    --user) user_setup ;;
    --help|-h) usage ;;
    "")
      install_system
      echo ""
      echo "System packages installed. As user s4il0r run:"
      echo "  bash $0 --user"
      echo "  cd $ROOT && ./scripts/build.sh"
      ;;
    --full)
      install_system
      # Run user parts as invoking user if sudo
      if [[ -n "${SUDO_USER:-}" ]]; then
        sudo -u "$SUDO_USER" HOME="$(getent passwd "$SUDO_USER" | cut -d: -f6)" bash "$0" --user
        sudo -u "$SUDO_USER" HOME="$(getent passwd "$SUDO_USER" | cut -d: -f6)" \
          bash -c "source $ENV_SNIPPET; cd '$ROOT' && ./scripts/build.sh"
      else
        user_setup
        build_lic
      fi
      ;;
    *) usage; exit 1 ;;
  esac
}

main "$@"
