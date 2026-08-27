# Li theorem layer (Layer 1) — math axioms in li

**Goal (Layer 1):** stop shipping the proof-database math layer only as Lean 4.
The math axioms now live as **li source** — `axiom` / `theorem` / `lemma`
declarations — parsed, typechecked, and counted by `lic` natively. Lean is no
longer the *only* home of the math; it becomes an export target (Layer 2
discharge replaces the AutoVC/lake round-trip next).

## What landed

- **Grammar:** `axiom`, `theorem`, `lemma` top-level declarations —
  `axiom name(params) : <bool proposition>` with `->` as implication
  (`peano_succ_injective(a: int, b: int) : (a + 1 == b + 1) -> a == b`).
- **Frontend:** lexer keywords, parser (`compiler/parser/parser.cpp`), AST
  `TheoremDecl` (`compiler/ast/include/li/ast.hpp`).
- **Typecheck:** propositions must be bool expressions (new **E0505**);
  theorem names share the top-level symbol space (duplicate check); params are
  bound locals (`compiler/types/typecheck.cpp`).
- **Verification surface:** `lic verify` reports `axioms=` / `theorems=` /
  `lemmas=`; `vcs.json` includes the counts.
- **Ported axioms** (`MathAxioms.lean` → li):
  - `proof-db/math/axioms/peano_order.li` — `peano_zero_not_succ`,
    `peano_succ_injective`, `order_trichotomy_nat`, `order_antisym`
    (M-AX-PEANO-*, M-AX-ORDER-*).
  - `proof-db/math/axioms/reals_field.li` — `real_add_comm`, `real_add_assoc`,
    `real_mul_distrib`, `real_mul_one` (M-AX-REAL-*).
  - `peano_induction` (M-AX-PEANO-IND) is second-order and stays a Lean axiom
    until higher-order support lands (noted in the file + catalog).
  - `proof-db/math/axioms/catalog.json` now maps each `M-AX-*`/`AX-MATH-*` id to
    its `li_axiom` name.
- **Tests:** `lexer_parser/theorems_parse.li` (parse_ok),
  `lexer_parser/theorems_bad_prop.li` (check_fail E0505),
  `proof_db_math` suite builds both axiom files end-to-end (`verify_ok`).

## Status

- `lic parse` / `lic check` / `lic verify` accept theorem files.
- Discharge of `theorem`/`lemma` propositions is **Layer 2** (native rewrite +
  arithmetic discharge in `lic`, then a li-syntax proof engine). Until then,
  `theorem`/`lemma` bodies are declarations without proofs — the `axiom`
  declarations are the only trusted statements, same policy as
  `docs/semantics/trusted.lean`.
