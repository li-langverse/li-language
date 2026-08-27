#!/usr/bin/env bash
# P-float: sqrt open bound — the abs(float) lemma VC must stay OPEN (Float
# lemmas pending) but the generated AutoVC must be valid Lean that lake
# builds: the ensures def binds `result` and no closing theorem is emitted.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/sqrt_open_bound.li"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
# --allow-open-vc: the abs lemma is a documented open gap, not a build error.
"$LIC" build --allow-open-vc "$SAMPLE" -o /dev/null
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
if "$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"; then
  echo "discharge_sqrt_open_lean: unexpected — sqrt abs VC should stay open" >&2
  exit 1
fi
grep -q 'result \* result' "$AUTOVC"
grep -q 'def vc_sqrt_open_ensures_0 (x : Float) (result : Float)' "$AUTOVC"
if command -v lake >/dev/null 2>&1; then
  cp "$AUTOVC" "$ROOT/docs/semantics/AutoVC.lean"
  (cd "$ROOT/docs/semantics" && lake build AutoVC)
  rm -f "$ROOT/docs/semantics/AutoVC.lean"
fi
echo "discharge_sqrt_open_lean: ok (abs VC open but AutoVC valid Lean)"
