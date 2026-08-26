# AutoVC fixes for default Lean gate (2f / open phases)

**Date:** 2026-05-21  
**PR:** #155 (branch `cursor/2i-7e-2f-57b4`)

## Summary

`lic build` default Lean verification no longer breaks on recursive `fib`, parallel-loop VCs, `f64` ensures, call-site requires with locals, or Lean-reserved param names like `by`.

## Agent continuation

1. **Read** `docs/verification/provability-gaps.md` (**G-lean**, **G-vc**) and master-plan tracker rows **2f / 2i / 7e / 7d**.
2. **Run** `./scripts/build.sh && ./li-tests/run_all.sh --ci` (needs `lake`; OpenMP optional — link skips `-fopenmp` when `/usr/include/omp.h` missing).
3. **Then** close **P-loop** (loop dot witness), **7d** `@parallel` MIR on `def`, **2i** broadcast policy; update gap register in same PR.
4. **Blocked on** kernel discharge for intentional open specimens (`sqrt_open_bound`, `linalg_dot4_int_loop_open`) until **G-trust** semantics land.

## Changed

| Area | Detail |
|------|--------|
| `compiler/verify/vc_emit_lean.cpp` | `decreases` → `Nat` (`Int.toNat`); call-site requires bind caller params + locals; parallel-for contracts use `_parN` suffix + loop iter param; `f64`/`f32` → `Float` in Lean types; escape Lean keywords (`by` → `by_`) |
| `compiler/verify/CMakeLists.txt` | Link `li_types` for `lookup_numeric_scalar` |
| `compiler/codegen/compile.cpp` | Skip `-fopenmp` on Linux when `omp.h` absent (serial `li_rt` fallback) |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Tracker: **2f** default lean, **7e** tier-1 advisory, **2i** float `@` partial |

## Not changed

- **G-lean Done** — still **Partial**; intentional open VCs unchanged.
- **7d** `@parallel` MIR elaboration on `def` — still parse/policy only.
- **2i** NumPy-style broadcast — still rejected / not implemented.
- **lip/lit** remote registry (**8d v2**) — out of scope.

## Breaking

N/A — AutoVC naming for parallel-loop obligations gains `_par0` suffix (generated Lean only).

## Security

N/A — proof emitter only; no trusted surface change.

## Performance

N/A — builds without system OpenMP still run parallel loops serially via `li_rt.c` `#else` branch.

## Downstream

Agents relying on `build/generated/AutoVC.lean` hand-edits must regenerate; CI `verify_ok` entries assume default Lean when `lake` is installed.
