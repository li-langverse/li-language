# 2i-b axpy/**/reductions, 2f axpy corpus, 7e tier-1 Li vs C++ reporter

## Summary

Closes the **2i-b** math surface slice (prelude `axpy`, same-length `**`, scalar×float-array, `math_linalg/reductions/`), grows **P-linalg** (`linalg_axpy4_int_closed`, `linalg_dot4_float_closed`), adds **`lic build --strict-lean`** (G-lean tier B), IKJ **`ArrayMatMul2DF64`** codegen, and **`check-tier1-li-vs-cpp.sh`** for tier-1 gaps.

## Agent continuation

1. **Read** `docs/verification/provability-gaps.md` § Still open; `scripts/check-tier1-li-vs-cpp.sh`; `li-tests/math_linalg/`.
2. **Run** `./scripts/build.sh && ./li-tests/run_all.sh math_linalg && ./li-tests/tooling/tier1_li_vs_cpp.sh`.
3. **Then** refresh `benchmarks/results/latest.csv` via `python3 benchmarks/harness/bench.py --tier 1`; close **G-math** tier-1 gaps (notably `matmul_naive`, `horner_pure_li`).
4. **Blocked on** float `@` Lean Props, 2D array CallProc codegen, default Lean kernel gate (**G-lean**).

## Changed

| Area | Paths / evidence |
|------|------------------|
| **2i-b** | `compiler/mir/lower.cpp` (`ArrayScaleF64`, `ArrayAxpyF64`, `**` in `ArrayBinOpF64`); `compiler/types/typecheck.cpp`, `prelude.cpp`; `li-tests/math_linalg/{elementwise_pow_float4,scale_float4,axpy_float4}.li`, `reductions/*.li` |
| **2f** | `linalg_dot4_float_closed` (`dot()` witness), `linalg_mat2_callproc_float_closed`; `--strict-lean` |
| **7e** | IKJ `ArrayMatMul2DF64`; release `-ffp-contract=fast`; matrix **CallProc** ABI (`MirParam.is_matrix`) |
| **2i** | `witness_dot4_prelude_call` in `vc_witness.cpp` / `vc_emit_lean.cpp` |
| **Docs** | `provability-gaps.md`, master plan, `proof-corpus-roadmap.md`, math-linalg plan |

## Not changed

- **G-lean** default kernel gate still off (use `--strict-lean` when lake is installed).
- Full 2×2 `@` **ensures** as Lean `Prop` (entry-only closed via `linalg_mat2_callproc_float_closed`).
- Tier-1 CSV not refreshed in-agent (`matmul_naive` bench run exceeds session budget); re-run `bench.py --tier 1` after merge.
- No `@parallel` MIR elaboration (**G-dec**).
- No org **benchmarks** catalog ingest (lic-local tier-1 CSV only).

## Breaking

N/A — additive prelude `axpy` and MIR ops; existing programs unchanged.

## Security

N/A — no trusted surface or CVE rows.

## Performance

`ArrayMatMul2DF64` uses IKJ accumulation (expected `matmul_naive` speedup; re-run `bench.py --tier 1` to refresh CSV). `horner_pure_li` still open. Tier-1 reporter unchanged (advisory by default).

## Downstream

N/A — no pin changes in `lip`/`lit`/org benchmarks ingest in this PR.
