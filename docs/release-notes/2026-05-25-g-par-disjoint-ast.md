# G-par: AST disjoint= validation (7d partial)

## Summary

`policy_module.cpp` now rejects weak parallel witnesses (`requires true`, bare `disjoint_row` / `disjoint_elem` names) and bare `@parallel(disjoint=disjoint_row)`; three new `race_shared_memory` compile-fail specimens lock the behavior.

## Agent continuation

1. Read `docs/verification/provability-gaps.md` (**G-par** row) and `compiler/types/policy_module.cpp`.
2. Run `./li-tests/run_all.sh race_shared_memory` after `cmake --build build`.
3. Next: Lean **P-par** proofs (7d-c); MIR `disjoint_proven` tagging is a separate **G-dec** slice.
4. Blocked: none for this PR.

## Changed

- `compiler/types/policy_module.cpp` — call-form loop `requires`; structured `disjoint=` on `@parallel` / `parallel for`
- `li-tests/race_shared_memory/false_disjoint_requires_true.li`
- `li-tests/race_shared_memory/false_disjoint_requires_bare_row.li`
- `li-tests/race_shared_memory/false_disjoint_decorator_bare_row.li`
- `li-tests/manifest.toml` — three `compile_fail` rows
- `docs/verification/provability-gaps.md` — **G-par** evidence (9+1 tests)

## Not changed

- Lean discharge for parallel VCs (**P-par**)
- `MirDecorator.disjoint_proven` / `check-mir-parallel-decorator.sh` (**G-dec**)
- OpenMP / codegen paths

## Breaking

N/A — compile-time rejections for invalid proofs only.

## Security

N/A — strengthens static rejection of unsound parallel witnesses; no trusted surface change.

## Performance

N/A — policy pass only.

## Downstream

N/A — `lic` compiler only.
