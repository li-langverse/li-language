#!/usr/bin/env bash
# Stage-2 bootstrap gate for Phase 6.
#
# This gate deliberately proves only the frontend boundary: the C++ host
# compiles bootstrap/lic/main.li, then the resulting Li binary lexes, parses,
# and dumps the AST of bootstrap/lic/main.li byte-for-byte like the C++ host.
# It must not be confused with full self-hosting: `lic-from-li build` is not
# implemented until the Layer 6 code generator/build driver is ported.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="$TMP/lic-from-li"
SELF="$ROOT/bootstrap/lic/main.li"

fail() {
  echo "check_li_stage2_frontend: $1" >&2
  exit 1
}

"$LIC" build "$SELF" -o "$LI" --allow-open-vc --no-lean-verify \
  >/dev/null 2>&1 || fail "C++ host could not build bootstrap/lic/main.li"

[[ -x "$LI" ]] || fail "stage-2 binary was not produced"

"$LIC" lex "$SELF" >"$TMP/cpp.lex" 2>/dev/null \
  || fail "C++ lexer rejected bootstrap/lic/main.li"
"$LI" lex "$SELF" >"$TMP/li.lex" 2>/dev/null \
  || fail "stage-2 lexer rejected bootstrap/lic/main.li"
diff -u "$TMP/cpp.lex" "$TMP/li.lex" >/dev/null \
  || fail "stage-2 lexer diverges on bootstrap/lic/main.li"

"$LIC" ast "$SELF" >"$TMP/cpp.ast" 2>/dev/null \
  || fail "C++ parser rejected bootstrap/lic/main.li"
"$LI" parse "$SELF" >"$TMP/li.parse" 2>/dev/null \
  || fail "stage-2 parser rejected bootstrap/lic/main.li"
"$LI" ast "$SELF" >"$TMP/li.ast" 2>/dev/null \
  || fail "stage-2 AST walker rejected bootstrap/lic/main.li"
diff -u "$TMP/cpp.ast" "$TMP/li.ast" >/dev/null \
  || fail "stage-2 AST dump diverges on bootstrap/lic/main.li"

echo "check_li_stage2_frontend: ok (stage-2 lexer + parser + AST parity on bootstrap/lic/main.li)"
echo "check_li_stage2_frontend: Layer 6 remains open (Li build/codegen is not yet self-hosted)"
