# Self-hosted lexer — the Li front end starts writing itself

**Date:** 2026-08-17
**Scope:** `bootstrap/lic/main.li`, `compiler/lexer`, `compiler/parser`, `compiler/mir/lower.cpp`, `compiler/codegen/emit.cpp`, `compiler/lic/main.cpp`, `runtime/li_rt.{c,h}`, `scripts/check_li_lexer_parity.sh`

## Summary

`bootstrap/lic/main.li` — the Li bootstrap binary compiled by the C++ host —
now contains a **real lexer written in Li**, and it produces the **exact same
token stream as the C++ Lexer** (`compiler/lexer`) on every `.li` file in the
repo: **492/492 files, exact token-kind + lexeme + accept/reject parity**.

This is Layer 2 of the self-hosting roadmap (Layer 1 = Li theorem layer /
`bootstrap/prover/main.li`). The lexer is gated by token-stream parity, not VC
discharge, so its CLI procs keep the documented `--allow-open-vc` build path
used by `scripts/bootstrap_lic.sh`.

## What was built

- **`lic lex <file>`** (C++ host, `compiler/lic/main.cpp`): dev/parity command
  dumping `kind<TAB>lexeme` per token using the `TokenKind` enum ordinals.
- **Runtime seams** (`runtime/li_rt.{c,h}`): `li_rt_read_file` (whole-file
  NUL-terminated read), `li_rt_str_len`, `li_rt_str_char_at`, `li_rt_str_eq`,
  `bytes_slice_i`, and `li_rt_emit_token` (emits one token line).
- **`lex_source` in `bootstrap/lic/main.li`**: a full port of the C++ lexer —
  the indentation state machine (`Indent`/`Dedent` stack, `=` body marker,
  `pending_indent_check`), comments, string literals, int/float/binary
  literals, keywords, and the operator table — using only `array[_, int]`
  state and the runtime seams, mirroring the bootstrap prover's idioms.
- **Parity gate** `scripts/check_li_lexer_parity.sh`: builds the Li lexer with
  the C++ host and diffs both token streams over an 8-file corpus (including
  the lexer itself and the prover) plus the tab-indent error path.

## C++ lexer quirks replicated exactly

- A `=` at line start **not** followed by a newline is *dropped* (the
  tokenizer resumes after it) — e.g. `= proof`.
- Tab indentation is rejected.
- Literal suffixes are consumed but excluded from the token text:
  `0.0f32` → FloatLit `0.0` (suffix `f32` dropped).
- `0b` with no binary digits falls through to a number literal.
- `ensures true` on value-returning procs is rejected (E0303); extern-calling
  procs must declare `raises IO` unless the calls are inside a loop.

## Compiler gaps found (and fixed) while self-hosting

1. **Parser: `else`/`elif` were lexed but never parsed.** `if` statements
   ignored everything after the then-block, so any `else:`/`elif:` produced
   "expected expression". The AST (`Stmt::else_body`), MIR lowering, typecheck,
   borrowck, and codegen already supported else-bodies — only the parser was
   missing the chain. Now `elif` desugars to nested `if`s and a trailing
   `else` closes the chain.

2. **Codegen: dead jump after a terminator.** With an else body, MIR emits
   `Jump merge_label` after the then-branch; if that branch ended in `return`,
   LLVM rejected the IR ("Terminator found in the middle of a basic block!").
   `MirOp::Jump` now skips the branch when the current block already
   terminates — which also fixes the same latent bug in while/for loops whose
   bodies end in `return`.

3. **Codegen: `print(<expr>)` printed nothing.** `lower_print_arg` only handled
   literal and ident arguments; a call or binop fell through with a default
   op, so `print(f(x))` silently emitted nothing. It now lowers general
   expressions to a temp and echoes the temp.

## Verification

- Full-repo sweep: **492/492 `.li` files, exact token parity** (kinds,
  lexemes, and accept/reject agreement), including empty files, `= proof`
  files, and missing-file behavior.
- `scripts/check_li_lexer_parity.sh`: ok (8 corpus files + error path).
- `li-tests`: effects 12/12, runtime 4/4, smoke 4/4 (incl. li-aimd),
  typecheck 8/8, lexer_parser 7/7; encapsulation 16 PASS / 0 FAIL before the
  run hit machine-load timeouts (CapCut saturating CPU) on a `lake` step.
- `check_li_prover_parity.sh`: cpp=22 li=18 open=0 (Layer 1 unchanged).
- Runtime checks: `print(1 + 2)` → `3`, `print(classify(0))` with
  if/elif/else chains → correct branch values.

## Remaining roadmap

- Layer 3: parser in Li (parity vs `lic parse`)
- Layer 4: typecheck + borrowck + effects in Li
- Layer 5: MIR lowering in Li
- Layer 6: LLVM codegen in Li, or stage-2 bootstrap (`lic` compiled by Li `lic`)
