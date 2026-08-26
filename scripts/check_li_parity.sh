#!/usr/bin/env bash
# One-command parity meta-gate for the self-hosted Li compiler port loop.
# Runs the four front-end parity gates (lexer, parser, AST, self-front-end)
# in dependency order and fails fast on the first divergence.
#
# Usage:
#   scripts/check_li_parity.sh                          # fast fixed-corpus gates
#   LI_PARITY_FULL_SWEEP=1 scripts/check_li_parity.sh   # full-repo parser/AST sweeps
#   LIC=/path/to/lic scripts/check_li_parity.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"

if [[ "${LI_PARITY_FULL_SWEEP:-0}" == "1" ]]; then
  export LI_PARSER_FULL_SWEEP=1
  export LI_AST_FULL_SWEEP=1
  echo "check_li_parity: full sweep mode (parser + AST over every *.li)"
else
  echo "check_li_parity: fast corpus mode (set LI_PARITY_FULL_SWEEP=1 for full sweeps)"
fi

# Resolve the host compiler once and reuse it across all four gates.
export LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

"$ROOT/scripts/check_li_lexer_parity.sh"
"$ROOT/scripts/check_li_parser_parity.sh"
"$ROOT/scripts/check_li_ast_parity.sh"
"$ROOT/scripts/check_li_self_frontend.sh"
"$ROOT/scripts/check_li_check_parity.sh"
"$ROOT/scripts/check_li_mir_parity.sh"

echo "check_li_parity: ok (lexer + parser + AST + self-front-end + check parity; MIR gate included)"
