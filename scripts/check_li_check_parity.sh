#!/usr/bin/env bash
# Layer 4 self-host parity: the li typechecker (`lic-from-li check`, built from
# bootstrap/lic/main.li) must agree with the C++ `lic check` verdict AND emit the
# same multiset of error codes (E0201/E0303/lic.error/...) on the typecheck +
# generics + effects + borrow corpus. This gates name resolution, type
# unification, numeric/width mixing, array bounds, protocol sizing, contract
# well-formedness (E0301/E0302/E0303), effect checking (raises IO/Alloc/Net/
# Async), borrow checking (E0310/E0311), and encapsulation policy (private
# field/method access, visibility, trait/object well-formedness, import
# resolution for field privacy) — not just accept/reject.
#
# Usage:
#   scripts/check_li_check_parity.sh
#   LI_CHECK_BIN=/custom/lic scripts/check_li_check_parity.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_CHECK_BIN:-$TMP/lic-from-li}"

fail() {
  echo "check_li_check_parity: $1" >&2
  exit 1
}

# Extract the sorted multiset of `[CODE]` tokens from a diagnostics stream.
codes_of() {
  grep -oE '\[[A-Za-z0-9.]+\]' "$1" | sort | tr '\n' ' ' || true
}

"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI" --allow-open-vc --no-lean-verify >/dev/null 2>&1 \
  || fail "could not build bootstrap/lic/main.li with $LIC"

CORPUS=()
for d in li-tests/typecheck li-tests/generics li-tests/effects li-tests/borrow \
         li-tests/encapsulation; do
  for f in "$ROOT/$d"/*.li; do
    [[ -f "$f" ]] || continue
    CORPUS+=("$f")
  done
done
[[ ${#CORPUS[@]} -gt 0 ]] || fail "no corpus files found under li-tests/{typecheck,generics,effects,borrow}"

total=0
for src in "${CORPUS[@]}"; do
  if "$LIC" check "$src" >"$TMP/cpp.out" 2>&1; then cpp=0; else cpp=1; fi
  if "$LI" check "$src" >"$TMP/li.out" 2>&1; then li=0; else li=1; fi
  cpp_codes="$(codes_of "$TMP/cpp.out")"
  li_codes="$(codes_of "$TMP/li.out")"
  total=$((total + 1))
  if [[ "$cpp" != "$li" ]]; then
    fail "verdict mismatch on ${src#$ROOT/} (lic=$cpp, lic-from-li=$li)"
  fi
  if [[ "$cpp_codes" != "$li_codes" ]]; then
    fail "error-code mismatch on ${src#$ROOT/} (lic=[$cpp_codes], lic-from-li=[$li_codes])"
  fi
  echo "  ok  ${src#$ROOT/} (verdict=$cpp codes=[$cpp_codes])"
done

echo "check_li_check_parity: ok ($total files, exact verdict + error-code parity)"
