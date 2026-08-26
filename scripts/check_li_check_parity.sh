#!/usr/bin/env bash
# Layer 4 self-host parity: the Li `check` subcommand (built from
# bootstrap/lic/main.li) must produce the same accept/reject verdicts
# as the C++ `lic check` on the typecheck+generics corpus.
#
# Usage:
#   scripts/check_li_check_parity.sh
#   LI_CHECK_BIN=/custom/lic scripts/check_li_check_parity.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_CHECK_BIN:-$TMP/lic-from-li}"

# Typecheck corpus: files that should be accepted
CORPUS_OK=(
  "li-tests/typecheck/fib.li"
  "li-tests/typecheck/let_bindings.li"
  "li-tests/typecheck/closures_basic.li"
  "li-tests/typecheck/closures_capture.li"
  "li-tests/typecheck/closures_higher_order.li"
  "li-tests/typecheck/closures_mutual.li"
  "li-tests/typecheck/records_basic.li"
  "li-tests/typecheck/records_init.li"
)

# Typecheck corpus: files that should be rejected with a specific error code
CORPUS_FAIL=(
  "li-tests/typecheck/undefined_var.li:E0304"
  "li-tests/typecheck/type_mismatch.li:E0201"
  "li-tests/typecheck/arity_mismatch.li:E0201"
)

checked=0
pass=0
fail=0

for f in "${CORPUS_OK[@]}"; do
  fp="$ROOT/$f"
  if [[ ! -f "$fp" ]]; then
    echo "  SKIP  $f (not found)"
    continue
  fi
  cpp_rc=0; li_rc=0
  "$LIC" check "$fp" >/dev/null 2>&1 || cpp_rc=$?
  "$LI" check "$fp" >/dev/null 2>&1 || li_rc=$?
  checked=$((checked + 1))
  if [[ "$cpp_rc" == "$li_rc" ]]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "  FAIL  $f  C++=$cpp_rc Li=$li_rc (expected both 0)"
  fi
done

for entry in "${CORPUS_FAIL[@]}"; do
  f="${entry%%:*}"
  expected_code="${entry##*:}"
  fp="$ROOT/$f"
  if [[ ! -f "$fp" ]]; then
    echo "  SKIP  $f (not found)"
    continue
  fi
  cpp_out=$( "$LIC" check "$fp" 2>&1 ) && cpp_rc=0 || cpp_rc=$?
  li_out=$( "$LI" check "$fp" 2>&1 ) && li_rc=0 || li_rc=$?
  checked=$((checked + 1))
  cpp_has=$(echo "$cpp_out" | grep -c "$expected_code" || true)
  li_has=$(echo "$li_out" | grep -c "$expected_code" || true)
  if [[ "$cpp_has" == "$li_has" && "$cpp_rc" != "0" && "$li_rc" != "0" ]]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "  FAIL  $f  expected $expected_code cpp=$cpp_rc/$cpp_has li=$li_rc/$li_has"
  fi
done

echo ""
echo "check_li_check_parity: $pass/$checked passed, $fail failed"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
