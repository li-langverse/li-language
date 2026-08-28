# Release notes: call-site callee `requires` VCs (2e/2f)

## Summary

`lic build` now emits Lean proof obligations for **callee `requires` at each in-module call site**, with auto-discharge when substituted preconditions are statically true (e.g. `callee(5)` with `requires x >= 0`).

## Agent continuation

1. **Read** `compiler/verify/vc_emit_lean.cpp` (`emit_call_site_requires`), `li-tests/contracts_verify/caller_requires_ok.li`, `docs/verification/provability-gaps.md` (**G-vc**).
2. **Run** `cmake --build build && ./li-tests/tooling/discharge_caller_requires_lean.sh && ./li-tests/tooling/contracts_discharge_corpus.sh && ./li-tests/tooling/contracts_verify_lean.sh`.
3. **Then** extend discharge to non-literal args (track locals/refinements), wire `LI_BUILD_VERIFY_LEAN_STRICT=1` on `lic build` when ready.
4. **Blocked on** float `ensures` with `abs` (`sqrt_open_bound.li` must stay open in corpus); full Lean kernel as default `lic build` gate (**G-lean**).

## Changed

| Path | Change |
|------|--------|
| `compiler/verify/vc_emit_lean.cpp` | Call-site callee `requires` → `AutoVC` defs; literal comparison discharge |
| `li-tests/contracts_verify/caller_requires_ok.li` | Regression: `callee(5)` closes `x >= 0` obligation |
| `li-tests/manifest.toml` | Register `caller_requires_ok.li` |
| `li-tests/tooling/discharge_caller_requires_lean.sh` | CI helper |
| `li-tests/tooling/contracts_verify_lean.sh` | Invoke caller-requires discharge |
| `docs/verification/provability-gaps.md` | **G-vc** row |
| `docs/language/contracts-and-proofs.md` | Partial call-site VCs |

## Not changed

- **`lic build`** still does not fail on open Lean goals by default (`LI_BUILD_VERIFY_LEAN_STRICT` opt-in).
- **Typecheck** does not yet prove caller args satisfy callee `requires` (VC-only today).
- **Cross-module / `extern` calls** — no call-site VCs.
- **Benchmarks**, **httpd M1**, **7e** perf.

## Breaking

N/A — additive VC emission; closed obligations get `_proved` theorems.

## Security

N/A — proof surface extension only.

## Performance

N/A — compile-time VC emit only.

## Downstream

None — monorepo `lic` only.
