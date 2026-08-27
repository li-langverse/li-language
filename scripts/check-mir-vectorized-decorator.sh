#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
[[ -x "$LIC" ]] || { echo "lic not built" >&2; exit 1; }
out="$("$LIC" verify "$ROOT/li-tests/decorators/vectorized_dot_proc_ok.li" 2>&1)"
echo "$out" | grep -q 'mir_vectorized_proc=1'
"$ROOT/li-tests/run_all.sh" decorators >/dev/null
echo check-mir-vectorized-decorator: ok
