#!/usr/bin/env bash
# Verify installed agent-kit matches committed expected version (CI-safe without sibling roadmap).
set -euo pipefail
LI_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXPECTED="$LI_ROOT/scripts/expected-agent-kit-version"
INSTALLED="$LI_ROOT/.cursor/agent-kit-version"
ROADMAP="${LI_ROADMAP_ROOT:-$LI_ROOT/../roadmap}"

if [[ -x "$ROADMAP/scripts/install-agent-kit.sh" ]]; then
  exec "$LI_ROOT/scripts/sync-agent-kit.sh" --check
fi

[[ -f "$EXPECTED" ]] || { echo "missing $EXPECTED — run ./scripts/sync-agent-kit.sh" >&2; exit 1; }
[[ -f "$INSTALLED" ]] || { echo "missing $INSTALLED — run ./scripts/sync-agent-kit.sh" >&2; exit 1; }
if [[ "$(cat "$EXPECTED")" != "$(cat "$INSTALLED")" ]]; then
  echo "agent-kit version mismatch: expected $(cat "$EXPECTED"), installed $(cat "$INSTALLED")" >&2
  exit 1
fi
echo "agent-kit ok: $(cat "$INSTALLED")"
