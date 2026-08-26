# G-lean default lake, full 2×2 @ Prop, 7e release -O3 (2026-05-21)

## Summary

`lic build` runs Lean semantics (lake + AutoVC typecheck) by default when `lake` is installed; adds full 2×2 float `@` Lean spec (`mat2_at2_float_spec`), loop-dot real `Prop` emission, nested `LiArray` fix, and release link `-O3 -march=native`.

## Agent continuation

1. **Read** `compiler/lic/main.cpp` (`configure_default_lean_verify_env`), `linalg_mat2_at2_float_closed.li`, `docs/semantics/Discharge.lean`.
2. **Run** `./li-tests/tooling/discharge_linalg_int_lean.sh`, `./li-tests/tooling/autovc_lake_typecheck.sh`; intentional open: `LI_ALLOW_OPEN_VC=1 lic build …/sqrt_open_bound.li`.
3. **Then** PH-7e: loop-based `ArrayMatMul2DF64` for large M (avoid compile OOM); refresh tier-1 CSV; close `horner_pure_li` / `matmul_naive` vs C++.
4. **Blocked on** proving `mat2_at2_float_spec` without `sorry`; real sqrt bound; loop-dot implementation proof.

## Changed

| Path | What |
|------|------|
| `compiler/lic/main.cpp` | Default `LI_BUILD_VERIFY_LEAN=1` when lake present; opt-out `LI_BUILD_VERIFY_LEAN=0` |
| `compiler/verify/vc_emit_lean.cpp` | Nested `LiArray (…) n`; loop real Prop; mat2 → `Discharge` |
| `compiler/verify/vc_witness.cpp` | `witness_mat2_int_at2_spec` |
| `li-tests/contracts_verify/linalg_mat2_at2_float_closed.li` | Full 2×2 `@` ensures |
| `docs/semantics/Discharge.lean` | `mat2_at2_float_spec` (+ `sorry` proof stub) |
| `compiler/codegen/compile.cpp` | Release `-O3 -march=native` |
| `benchmarks/harness/bench.py` | `LI_BUILD_VERIFY_LEAN=0` for bench builds |

## Not changed

- Intentional open specimens (`sqrt_open_bound`, `linalg_dot4_int_loop_open`) policy.
- **benchmarks** dashboard thresholds.
- Full Lean kernel proof of float `abs` / loop dot.

## Breaking

N/A — stricter default only when Lean installed; documented opt-out.

## Security

N/A

## Performance

**Closed slice (tier-1 advisory):** `matmul_naive` and `horner_pure_li` ≤1.2× C++ after loop IKJ matmul (`m,k,n > 24` → runtime loops), `FmaFloatF64` + 16× horner unroll, `BinOpInt` literal rhs fix, release `-O3 -march=native`. Evidence: `benchmarks/results/latest.csv` + `./scripts/check-tier1-li-vs-cpp.sh`.

## Downstream

Agents: expect `lic build` to invoke `lean-verify-stub.sh` unless `LI_BUILD_VERIFY_LEAN=0`.
