#!/usr/bin/env bash
# Layer 2c self-host parity: the li AST dump (bootstrap/lic/main.li `ast`,
# compiled by `lic build`) must be byte-identical to the C++ `lic ast` dump
# (compiler/ast/ast_dump.{hpp,cpp}) on every file that parses.
#
# The `ast` subcommand emits an int-encoded canonical pre-order dump of the
# parse tree, one node per line. Tree-shape parity — not just accept/reject —
# proves the Li parser builds the same tree as the C++ parser.
#
# Usage:
#   scripts/check_li_ast_parity.sh
#   LI_AST_BIN=/custom/lic scripts/check_li_ast_parity.sh
#   LI_AST_FULL_SWEEP=1 scripts/check_li_ast_parity.sh   # every *.li in the repo
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI_AST="${LI_AST_BIN:-$TMP/lic-from-li}"

CORPUS=(
  "bootstrap/lic/main.li"
  "bootstrap/prover/main.li"
  "examples/hello.li"
  "examples/arrays.li"
  "li-tests/effects/io_ok.li"
  "packages/li-aimd/src/lib.li"
  "packages/li-chem/src/lib.li"
  "packages/li-net-httpd/src/lib.li"
  "packages/li-render/src/lib.li"
  "packages/li-studio/src/lib.li"
  "packages/lig/src/lib.li"
  "proof-db/math/lemmas/ring_discharge.li"
  "li-tests/lexer_parser/parser_accept_elif.li"
  "li-tests/parallel_codegen/parallel_float_zero.li"
  "li-tests/encapsulation/trait_hash_impl.li"
  "li-tests/collections/typedict_ok.li"
  "li-tests/collections/enum_ok.li"
  "li-tests/collections/tuple_variadic.li"
  "li-tests/collections/tuple_pair.li"
  "li-tests/encapsulation/inheritance_layout.li"
  "li-tests/prob/collision_oracle.li"
  "li-tests/lexer_parser/decorators_parse.li"
  "li-tests/lexer_parser/async_await_parse.li"
  "li-tests/typecheck/binary_literal_ok.li"
  "li-tests/log/redact_bearer.li"
)

fail() {
  echo "check_li_ast_parity: $1" >&2
  exit 1
}

# Build the li lexer+parser with the C++ host (the self-host milestone gate).
"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI_AST" --allow-open-vc >/dev/null 2>&1 \
  || fail "could not build bootstrap/lic/main.li with $LIC"

checked=0
for f in "${CORPUS[@]}"; do
  src="$ROOT/$f"
  [[ -f "$src" ]] || fail "corpus file missing: $f"
  if ! "$LIC" ast "$src" > "$TMP/cpp_ast.txt" 2>/dev/null; then
    fail "C++ parser rejected corpus file $f (gate corpus must parse)"
  fi
  if ! "$LI_AST" ast "$src" > "$TMP/li_ast.txt" 2>/dev/null; then
    fail "li parser rejected $f (parity break on valid input)"
  fi
  if ! diff -q "$TMP/cpp_ast.txt" "$TMP/li_ast.txt" >/dev/null 2>&1; then
    echo "check_li_ast_parity: AST mismatch on $f:" >&2
    diff "$TMP/cpp_ast.txt" "$TMP/li_ast.txt" | head -15 >&2
    fail "li AST dump differs from C++ on $f"
  fi
  checked=$((checked + 1))
  echo "  ok  $f ($(wc -l < "$TMP/cpp_ast.txt") nodes)"
done

if [[ "${LI_AST_FULL_SWEEP:-0}" == "1" ]]; then
  checked=0
  while IFS= read -r f; do
    cpp=0; li=0
    "$LIC" ast "$f" >/dev/null 2>&1 && cpp=1
    "$LI_AST" ast "$f" >/dev/null 2>&1 && li=1
    if [[ "$cpp" != "$li" ]]; then
      fail "accept/reject divergence on $f (cpp=$cpp li=$li)"
    fi
    if [[ "$cpp" == "1" ]]; then
      "$LIC" ast "$f" > "$TMP/cpp_ast.txt" 2>/dev/null
      "$LI_AST" ast "$f" > "$TMP/li_ast.txt" 2>/dev/null
      if ! diff -q "$TMP/cpp_ast.txt" "$TMP/li_ast.txt" >/dev/null 2>&1; then
        fail "AST mismatch on $f"
      fi
    fi
    checked=$((checked + 1))
  done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
             "$ROOT/proof-db" "$ROOT/benchmarks" "$ROOT/docs" -name "*.li" -type f 2>/dev/null)
  echo "  full sweep: $checked files, exact AST parity"
fi

echo "check_li_ast_parity: ok (${#CORPUS[@]} corpus files, byte-exact AST dumps)"
