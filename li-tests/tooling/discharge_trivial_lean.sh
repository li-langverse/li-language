#!/usr/bin/env bash
# 2f: AutoVC from discharge_trivial.li must have zero open Prop goals; lake build when available.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/discharge_trivial.li"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
"$LIC" build "$SAMPLE" -o /dev/null
test -f "$AUTOVC"
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
grep -q 'Phase 2f: return expression matches ensures' "$AUTOVC" || {
  echo "discharge_trivial_lean: missing static witness comments in AutoVC"
  exit 1
}
"$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"
if command -v lake >/dev/null 2>&1; then
  (cd "$ROOT/docs/semantics" && lake build)
  echo "discharge_trivial_lean: lake ok"
else
  echo "discharge_trivial_lean: skipped lake (not installed)"
fi
echo "discharge_trivial_lean: ok"
