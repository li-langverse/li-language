#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
"$LIC" build "$ROOT/li-tests/contracts_verify/refinement_call_ok.li" -o /dev/null
test -f "$AUTOVC"
grep -q 'P-refine folded:' "$AUTOVC"
grep -q '_refine_.*_proved' "$AUTOVC"
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
"$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"
echo "discharge_refinement_lean: ok"
