#!/usr/bin/env bash
# Stage-2 first step: the li-compiled front end (`lic-from-li`, built from
# bootstrap/lic/main.li) must process its OWN source — lex, parse, and AST —
# byte-identically to the C++ compiler. This is the front-end half of the
# stage-2 self-build: once the backend (typecheck/MIR/codegen) is ported, the
# same binary will compile itself. This gate proves the front end is already
# self-hosting.
#
# Usage:
#   scripts/check_li_self_frontend.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_BIN:-$TMP/lic-from-li}"

fail() {
  echo "check_li_self_frontend: $1" >&2
  exit 1
}

# Build lic-from-li with the C++ host.
"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI" --allow-open-vc >/dev/null 2>&1 \
  || fail "could not build bootstrap/lic/main.li with $LIC"

SRC="$ROOT/bootstrap/lic/main.li"

# 1. Token-stream parity of its own source.
"$LIC" lex "$SRC" > "$TMP/cpp_lex.txt" 2>/dev/null
"$LI" lex "$SRC" > "$TMP/li_lex.txt" 2>/dev/null
if ! diff -q "$TMP/cpp_lex.txt" "$TMP/li_lex.txt" >/dev/null 2>&1; then
  diff "$TMP/cpp_lex.txt" "$TMP/li_lex.txt" | head -10 >&2
  fail "self lex: token stream of bootstrap/lic/main.li differs from C++"
fi
echo "  ok  self lex ($(wc -l < "$TMP/cpp_lex.txt") tokens)"

# 2. Parse verdict (accept) + token-stream the parser consumed.
"$LI" parse "$SRC" > "$TMP/li_parse.txt" 2>/dev/null \
  || fail "self parse: lic-from-li rejected its own source"
tail -n 1 "$TMP/li_parse.txt" | grep -qx "parse ok" \
  || fail "self parse: verdict not 'parse ok'"
if ! diff -q "$TMP/cpp_lex.txt" <(sed '$d' "$TMP/li_parse.txt") >/dev/null 2>&1; then
  fail "self parse: consumed token stream differs from C++"
fi
echo "  ok  self parse"

# 3. AST dump of its own source, byte-identical to C++.
"$LIC" ast "$SRC" > "$TMP/cpp_ast.txt" 2>/dev/null
"$LI" ast "$SRC" > "$TMP/li_ast.txt" 2>/dev/null
if ! diff -q "$TMP/cpp_ast.txt" "$TMP/li_ast.txt" >/dev/null 2>&1; then
  diff "$TMP/cpp_ast.txt" "$TMP/li_ast.txt" | head -10 >&2
  fail "self ast: dump of bootstrap/lic/main.li differs from C++"
fi
echo "  ok  self ast ($(wc -l < "$TMP/cpp_ast.txt") nodes)"

# 4. The prover bootstrap must also lex/parse/ast identically.
PSRC="$ROOT/bootstrap/prover/main.li"
"$LIC" ast "$PSRC" > "$TMP/cpp_past.txt" 2>/dev/null
"$LI" ast "$PSRC" > "$TMP/li_past.txt" 2>/dev/null
if ! diff -q "$TMP/cpp_past.txt" "$TMP/li_past.txt" >/dev/null 2>&1; then
  fail "self ast: bootstrap/prover/main.li differs from C++"
fi
echo "  ok  self ast (bootstrap/prover/main.li)"

echo "check_li_self_frontend: ok (lic-from-li parses its own front end byte-identically)"
