# G-dec: MIR decorator corpus gate (partial)

**Date:** 2026-05-25  
**Gaps:** **G-dec** (Partial), **P-dec** (Partial)  
**Phase:** 7d, 2f

## Summary

Adds `@parallel` / `@vectorized` MIR proc metadata, `lic verify` telemetry (`mir_parallel_disjoint=`, `mir_vectorized_proc=`), and wires `check-mir-*-decorator.sh` into the 2f discharge corpus.

## Agent continuation

1. Read `docs/verification/provability-gaps.md` (**G-dec**).
2. Run `./scripts/build.sh && ./scripts/check-mir-parallel-decorator.sh && ./scripts/check-mir-vectorized-decorator.sh`.
3. Supersedes closed duplicate PRs **#193**, **#201**, **#202** — do not reopen.
4. Blocked: Lean **P-dec** proofs.

## Changed

- `compiler/mir/*`, `compiler/lic/main.cpp`
- `scripts/check-mir-{parallel,vectorized}-decorator.sh`
- `li-tests/tooling/contracts_discharge_corpus.sh`, `scripts/check-master-plan-gates.sh`
- `docs/verification/provability-gaps.md`

## Not changed

- **G-par** AST policy — unchanged.
- Lean **P-dec** — open.

## Breaking / Security / Performance / Downstream

N/A
