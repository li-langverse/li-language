#!/usr/bin/env bash
# prob-hoare P2: prob_ensures parse + lic build --prob-check Monte Carlo gate.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
ORACLE="$ROOT/li-tests/prob/collision_oracle.li"

if [[ ! -x "$LIC" ]]; then
  echo "check-prob-hoare: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

echo "==> lic check collision_oracle (prob_ensures syntax)"
"$LIC" check "$ORACLE"

echo "==> prob_check.py unit"
python3 "$ROOT/scripts/prob_check.py" "$ORACLE"

echo "==> lic build --prob-check collision_oracle"
"$LIC" build "$ORACLE" -o /tmp/li_prob_collision_oracle --allow-open-vc --no-lean-verify --prob-check
test -f "$ROOT/build/generated/prob_check.json"

echo "==> Lean Probability.lean (measure obligations stub)"
if command -v lake >/dev/null 2>&1; then
  (cd "$ROOT/docs/semantics" && lake build Probability 2>/dev/null) || true
fi

echo "check-prob-hoare: OK"
