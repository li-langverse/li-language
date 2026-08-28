#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
export LI_KEEP_LL=1
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/bounds_refinement_release_ok.li"
LL="$ROOT/build/last_emit.ll"
rm -f "$LL"
"$LIC" build --release "$SAMPLE" -o /dev/null
[[ -f "$LL" ]]
! grep -q 'call void @li_bounds_fail' "$LL"
grep -q 'getelementptr inbounds' "$LL"
echo "check_release_bounds_ir: ok"
