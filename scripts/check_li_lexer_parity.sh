#!/usr/bin/env bash
# Layer 2 self-host parity: the li lexer (bootstrap/lic/main.li, compiled by
# `lic build`) must emit the exact same token stream as the C++ Lexer
# (compiler/lexer) for every file in the corpus.
#
# The lexer is milestone 1 of the self-hosted front end: it is written in li,
# built by the C++ host (`lic build`), and gated by token-stream parity rather
# than VC discharge, so its CLI procs may leave the documented
# `ensures result == 0 or result == 1` VCs open (hence --allow-open-vc here).
#
# Usage:
#   scripts/check_li_lexer_parity.sh
#   LI_LEXER_BIN=/custom/lic scripts/check_li_lexer_parity.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI_LEXER="$TMP/lic-from-li"

CORPUS=(
  "bootstrap/lic/main.li"
  "bootstrap/prover/main.li"
  "examples/hello.li"
  "li-tests/effects/io_ok.li"
  "packages/li-aimd/src/lib.li"
  "packages/li-chem/src/lib.li"
  "packages/li-net-httpd/src/lib.li"
  "proof-db/math/lemmas/ring_discharge.li"
)

fail() {
  echo "check_li_lexer_parity: $1" >&2
  exit 1
}

# Build the li lexer with the C++ host (the self-host milestone gate).
"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI_LEXER" --allow-open-vc >/dev/null 2>&1 \
  || fail "could not build bootstrap/lic/main.li with $LIC"

for f in "${CORPUS[@]}"; do
  src="$ROOT/$f"
  [[ -f "$src" ]] || fail "corpus file missing: $f"
  "$LIC" lex "$src" > "$TMP/cpp.txt" 2>/dev/null || fail "C++ lexer rejected $f"
  "$LI_LEXER" lex "$src" > "$TMP/li.txt" 2>/dev/null || fail "li lexer rejected $f (parity break on valid input)"
  if ! diff -q "$TMP/cpp.txt" "$TMP/li.txt" >/dev/null 2>&1; then
    echo "check_li_lexer_parity: token mismatch on $f:" >&2
    diff "$TMP/cpp.txt" "$TMP/li.txt" | head -10 >&2
    fail "li lexer diverged from C++ lexer on $f"
  fi
  echo "  ok  $f ($(wc -l < "$TMP/cpp.txt") tokens)"
done

# Error path: a lexically-invalid file (tab indentation) must be rejected by
# both implementations.
printf 'def main() -> int\n\treturn 1\n' > "$TMP/bad.li"
if "$LIC" lex "$TMP/bad.li" >/dev/null 2>&1; then
  fail "C++ lexer unexpectedly accepted tab-indent file"
fi
if "$LI_LEXER" lex "$TMP/bad.li" >/dev/null 2>&1; then
  fail "li lexer accepted tab-indent file (C++ rejects it)"
fi

echo "check_li_lexer_parity: ok (${#CORPUS[@]} corpus files, error path, exact token parity)"
