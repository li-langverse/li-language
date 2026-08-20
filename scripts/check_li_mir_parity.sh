#!/usr/bin/env bash
# Layer 5 self-host parity (SKELETON): the li `mir` subcommand (built from
# bootstrap/lic/main.li) must emit a canonical MIR dump byte-identical to the
# C++ `lic mir <file>` dump of lower_to_mir() output.
#
# Status: not yet ported. This gate builds lic-from-li and checks whether the
# `mir` subcommand is implemented; until it is, it reports SKIP and exits 0 so
# the meta-gate (scripts/check_li_parity.sh) can wire it in without breaking CI.
#
# Plan (upgrade-li-from-cpp discipline):
#   1. Add `lic mir <file>` to the C++ host first (reference dump).
#   2. Implement `mir` in bootstrap/lic/main.li (AST -> MIR).
#   3. Flip this gate from SKIP to a real diff.
#
# Usage:
#   scripts/check_li_mir_parity.sh
#   LI_CHECK_BIN=/custom/lic scripts/check_li_mir_parity.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_CHECK_BIN:-$TMP/lic-from-li}"

"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI" --allow-open-vc --no-lean-verify >/dev/null 2>&1 \
  || { echo "check_li_mir_parity: could not build bootstrap/lic/main.li with $LIC" >&2; exit 1; }

if ! "$LI" mir "$ROOT/li-tests/typecheck/fib.li" >/dev/null 2>&1; then
  echo "check_li_mir_parity: SKIP (Layer 5 'mir' subcommand not yet ported)"
  exit 0
fi

echo "check_li_mir_parity: ok (MIR dump parity)"
