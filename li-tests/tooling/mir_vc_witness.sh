#!/usr/bin/env bash
# 2e: lic verify reports MIR-linked witnessed ensures.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
out="$("$LIC" verify "$ROOT/li-tests/contracts_verify/discharge_const.li" 2>&1)"
echo "$out" | grep -q 'mir_fns=1'
echo "$out" | grep -q 'witnessed_ensures=1'
echo "$out" | grep -q 'mir_return_linked=1'
echo "mir_vc_witness: ok"
