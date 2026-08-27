#!/usr/bin/env bash
# Layer 2b self-host parity: the li parser (bootstrap/lic/main.li, compiled by
# `lic build`) must accept / reject exactly the files the C++ Parser
# (compiler/parser) does, and the token stream it consumes must equal the C++
# lexer's stream — token-stream parity against the C++ parser.
#
# `lic-from-li parse <file>` re-dumps the token stream it consumed (same
# format as `lic lex`) and then prints `parse ok` / `parse error`, exiting
# 0 / 1 like `lic parse`. The gate diffs the consumed stream against `lic lex`
# and the accept/reject verdicts against `lic parse`, on a corpus that covers
# every grammar feature: imports, externs, decorators, visibility, traits,
# typedicts, enums, objects, tuples, simd, Callable, refinements, theorems /
# axioms / lemmas, parallel-for, for, while, if/elif/else, borrow, discard,
# contracts incl. prob_ensures, and error decls.
#
# Usage:
#   scripts/check_li_parser_parity.sh
#   LI_PARSER_BIN=/custom/lic scripts/check_li_parser_parity.sh
#   LI_PARSER_FULL_SWEEP=1 scripts/check_li_parser_parity.sh   # every *.li in the repo
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI_PARSER="${LI_PARSER_BIN:-$TMP/lic-from-li}"

ACCEPT_CORPUS=(
  "bootstrap/lic/main.li"
  "bootstrap/prover/main.li"
  "examples/hello.li"
  "li-tests/effects/io_ok.li"
  "packages/li-aimd/src/lib.li"
  "packages/li-chem/src/lib.li"
  "packages/li-net-httpd/src/lib.li"
  "packages/li-render/src/lib.li"
  "packages/li-studio/src/lib.li"
  "proof-db/math/lemmas/ring_discharge.li"
  "li-tests/lexer_parser/parser_accept_elif.li"
  "li-tests/parallel_codegen/parallel_float_zero.li"
  "li-tests/encapsulation/trait_hash_impl.li"
  "li-tests/collections/typedict_ok.li"
  "li-tests/collections/enum_ok.li"
  "li-tests/collections/tuple_variadic.li"
  "li-tests/encapsulation/inheritance_layout.li"
  "li-tests/prob/collision_oracle.li"
  "li-tests/lexer_parser/decorators_parse.li"
  "li-tests/lexer_parser/async_await_parse.li"
)

REJECT_CORPUS=(
  "li-tests/lexer_parser/parser_reject_proc_kw.li"
  "li-tests/lexer_parser/parser_reject_no_body.li"
  "li-tests/lexer_parser/parser_reject_unclosed_call.li"
  "li-tests/lexer_parser/parser_reject_bad_paren.li"
  "li-tests/lexer_parser/parser_reject_bad_expr.li"
)

fail() {
  echo "check_li_parser_parity: $1" >&2
  exit 1
}

# Build the li lexer+parser with the C++ host (the self-host milestone gate).
"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI_PARSER" --allow-open-vc >/dev/null 2>&1 \
  || fail "could not build bootstrap/lic/main.li with $LIC"

for f in "${ACCEPT_CORPUS[@]}"; do
  src="$ROOT/$f"
  [[ -f "$src" ]] || fail "corpus file missing: $f"
  "$LIC" parse "$src" >/dev/null 2>&1 \
    || fail "C++ parser rejected corpus file $f (gate corpus must parse)"
  # The li parser must accept AND its consumed token stream must be the
  # exact stream the C++ parser sees (`lic lex`).
  "$LI_PARSER" parse "$src" > "$TMP/li_parse.txt" 2>/dev/null \
    || fail "li parser rejected $f (parity break on valid input)"
  tail -n 1 "$TMP/li_parse.txt" | grep -qx "parse ok" \
    || fail "li parser verdict not 'parse ok' on $f"
  "$LIC" lex "$src" > "$TMP/cpp_tok.txt" 2>/dev/null
  if ! diff -q "$TMP/cpp_tok.txt" <(sed '$d' "$TMP/li_parse.txt") >/dev/null 2>&1; then
    echo "check_li_parser_parity: token-stream mismatch on $f:" >&2
    diff "$TMP/cpp_tok.txt" <(sed '$d' "$TMP/li_parse.txt") | head -10 >&2
    fail "li parser consumed a different token stream than the C++ parser on $f"
  fi
  echo "  ok  $f ($(wc -l < "$TMP/cpp_tok.txt") tokens)"
done

for f in "${REJECT_CORPUS[@]}"; do
  src="$ROOT/$f"
  [[ -f "$src" ]] || fail "reject corpus file missing: $f"
  if "$LIC" parse "$src" >/dev/null 2>&1; then
    fail "C++ parser unexpectedly accepted reject-case $f"
  fi
  if "$LI_PARSER" parse "$src" >/dev/null 2>&1; then
    fail "li parser accepted reject-case $f (C++ rejects it)"
  fi
  echo "  reject  $f"
done

# Tab-indent is a *lexer* error both implementations must reject at parse time.
printf 'def main() -> int\n\treturn 1\n' > "$TMP/bad_indent.li"
if "$LIC" parse "$TMP/bad_indent.li" >/dev/null 2>&1; then
  fail "C++ parser accepted tab-indent file"
fi
if "$LI_PARSER" parse "$TMP/bad_indent.li" >/dev/null 2>&1; then
  fail "li parser accepted tab-indent file"
fi

if [[ "${LI_PARSER_FULL_SWEEP:-0}" == "1" ]]; then
  checked=0
  while IFS= read -r f; do
    cpp=0; li=0
    "$LIC" parse "$f" >/dev/null 2>&1 && cpp=1
    "$LI_PARSER" parse "$f" >/dev/null 2>&1 && li=1
    if [[ "$cpp" != "$li" ]]; then
      fail "accept/reject divergence on $f (cpp=$cpp li=$li)"
    fi
    checked=$((checked + 1))
  done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
             "$ROOT/proof-db" "$ROOT/benchmarks" "$ROOT/docs" -name "*.li" -type f 2>/dev/null)
  echo "  full sweep: $checked files, exact accept/reject parity"
fi

echo "check_li_parser_parity: ok (${#ACCEPT_CORPUS[@]} accept + ${#REJECT_CORPUS[@]} reject corpus, error path, token-stream + accept/reject parity)"
