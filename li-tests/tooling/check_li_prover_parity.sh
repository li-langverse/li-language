#!/usr/bin/env bash
# Layer 3 self-host parity: the li prover (bootstrap/prover/main.li, built by
# `lic build`) must close the rewriting half of ring_discharge.li on its own,
# and the combined build gate (C++ rewriting+LIA engine OR li prover) must
# stay fully closed.
#
# The li prover deliberately does NOT run linear integer arithmetic
# (Fourier-Motzkin stays a compiler builtin, like `omega` in Lean), so the
# rewriting corpus baseline is 18 of 22; the 4 LIA theorems close through the
# C++ engine. Raise LI_PROVER_MIN to 22 once LIA is ported into the li prover.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
LI_PROVER_MIN="${LI_PROVER_MIN:-18}"

SAMPLE="$ROOT/proof-db/math/lemmas/ring_discharge.li"
OUT="$("$LIC" verify "$SAMPLE" 2>/dev/null)"

proved="$(echo "$OUT" | sed -n 's/.*theorems_proved=\([0-9]*\).*/\1/p')"
li_proved="$(echo "$OUT" | sed -n 's/.*theorems_li_proved=\([0-9]*\).*/\1/p')"
open="$(echo "$OUT" | sed -n 's/.*theorems_open=\([0-9]*\).*/\1/p')"

fail() {
  echo "check_li_prover_parity: $1" >&2
  echo "  verify output: $OUT" >&2
  exit 1
}

[[ -n "$proved" ]] || fail "could not parse theorems_proved from lic verify"
[[ -n "$li_proved" ]] || fail "could not parse theorems_li_proved from lic verify"
[[ -n "$open" ]] || fail "could not parse theorems_open from lic verify"

[[ "$open" == "0" ]] || fail "combined build gate left $open proposition(s) open"
[[ "$proved" == "22" ]] || fail "C++ engine closed $proved/22 (expected 22)"
[[ "$li_proved" -ge "$LI_PROVER_MIN" ]] || {
  fail "li prover closed $li_proved/$LI_PROVER_MIN — self-host prover regressed (build/prover/li_prover missing or broken?)"
}

echo "check_li_prover_parity: ok (cpp=$proved li=$li_proved open=$open)"
