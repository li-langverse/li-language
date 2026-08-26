#!/usr/bin/env bash
# Install Lean 4 via elan (user-local). Idempotent.
# Usage: bash scripts/ci-install-lean.sh
# After: export PATH="$HOME/.elan/bin:$PATH"
set -euo pipefail

if command -v lake >/dev/null 2>&1; then
  echo "lean: already installed ($(lake --version | head -1))"
  exit 0
fi

curl -sSf https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh | sh -s -- -y --default-toolchain stable
export PATH="${HOME}/.elan/bin:${PATH}"
if ! command -v lake >/dev/null 2>&1; then
  echo "lean: elan installed but lake not on PATH — add: export PATH=\"\$HOME/.elan/bin:\$PATH\"" >&2
  exit 1
fi
lake --version
echo "lean: ok — add to shell profile: export PATH=\"\$HOME/.elan/bin:\$PATH\""
