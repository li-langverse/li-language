# G-lean: LiArray lake typecheck (2026-05-21)

## Summary

Generated `AutoVC.lean` now uses `LiArray α n` from `docs/semantics/Core.lean` so `lake build AutoVC` typechecks P-linalg specimens when Lean 4 is installed.

## Agent continuation

1. **Read** `docs/semantics/Core.lean`, `compiler/verify/vc_emit_lean.cpp`, `docs/verification/provability-gaps.md` (**G-lean**).
2. **Run** `elan` + `lake` if missing; `./li-tests/tooling/autovc_lake_typecheck.sh`, `./li-tests/tooling/discharge_linalg_int_lean.sh`, `./li-tests/tooling/glean_strict_build_smoke.sh`.
3. **Then** refresh tier-1 CSV (`python3 benchmarks/harness/bench.py --tier 1`); close **G-math** `matmul_naive` / `horner_pure_li` vs C++ (≤1.2×).
4. **Blocked on** default kernel discharge for intentional open specimens (`sqrt_open_bound`, real float `ensures`).

## Changed

| Path | What |
|------|------|
| `docs/semantics/Core.lean` | `LiArray`, `GetElem` for `[i]!` in discharge lemmas |
| `compiler/verify/vc_emit_lean.cpp` | Emit `LiArray`, `open Li` in AutoVC header |
| `docs/semantics/Discharge.lean` | Imports first; `LiArray` in `dot4_int_spec` |
| `scripts/lean-verify-stub.sh` | `lake build AutoVC Discharge` when generated file exists |
| `li-tests/tooling/autovc_lake_typecheck.sh` | New smoke |
| `scripts/check-master-plan-gates.sh`, `.github/workflows/lean.yml` | Wire lake AutoVC gate |
| **PH-2f** / **G-lean** | Lake typecheck slice (not full kernel default) |

## Not changed

- Default `lic build` still does not require Lean unless `--strict-lean` / env.
- **lic** Horner SIMD / FMA codegen (**7e** `horner_pure_li`).
- **benchmarks** repo ingest or dashboard thresholds.
- Intentional open VC specimens (`sqrt_open_bound`, loop-dot real Props).

## Breaking

N/A — proof-layer naming only; no Li source syntax change.

## Security

N/A — no trusted surface or CVE rows.

## Performance

N/A — no codegen path change in this note.

## Downstream

N/A — agents with local `lake` should re-run `lake build` in `docs/semantics` after pulling.
