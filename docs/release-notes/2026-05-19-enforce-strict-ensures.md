# Release notes: enforce strict `ensures` on value-returning `def`

## Summary

`lic` now rejects `ensures true` on procedures that return a value (`int`, `float`, structs, etc.) with **E0303**, and official packages use postconditions that relate `result` to the computation.

## Agent continuation

1. **Read** `docs/ecosystem/strict-by-default.md`, `docs/language/contracts-and-proofs.md`, `compiler/types/typecheck.cpp` (`check_weak_ensures`), `li-tests/prove_reject/weak_ensures_true.li`.
2. **Run** `cmake --build build && ./li-tests/run_all.sh prove_reject contracts_verify composable && ./scripts/lic-workspace-build.sh`.
3. **Then** tighten any new public `def` with `ensures result == …` or honest bounds; use `scripts/tighten-weak-ensures.py` only for bulk migration (review field-level specs).
4. **Blocked on** full Lean discharge of non-literal `ensures` (G-lean partial); float VCs still open per `docs/verification/provability-gaps.md`.

## Changed

| Path | Change |
|------|--------|
| `compiler/diagnostics/include/li/error_codes.hpp` | **E0303** `contract.weak_ensures_true` |
| `compiler/types/typecheck.cpp` | Reject `ensures true` when return type is not `unit` (non-`extern`) |
| `li-tests/prove_reject/weak_ensures_true.li` | `compile_fail` corpus |
| `li-tests/manifest.toml` | Register prove_reject case |
| `packages/*/src/lib.li` | Replace weak `ensures true` with `result == …` or domain bounds |
| `scripts/tighten-weak-ensures.py` | One-shot migration helper (review output) |
| `docs/language/contracts-and-proofs.md` | Document E0303 |

## Not changed

- **`extern proc`** may still use `ensures true` for opaque runtime FFI.
- **`-> unit`** procedures may still use `ensures true` (stubs / void effects).
- **Lean 4** auto-discharge maturity (G-lean) — static gate only.
- **Benchmarks repo** ingest / dashboard thresholds.
- **`li.toml` `[gates]`** downgrade schema — not wired yet.

## Breaking

| Item | Migration |
|------|-----------|
| **E0303** on `ensures true` + non-`unit` return | Use `ensures result == <expr>` or properties on `result` (e.g. `ensures result >= 0.0`). |

## Security

N/A — contract strictness only; no trusted surface change.

## Performance

N/A — typecheck-only gate.

## Downstream

Org package mirrors: re-sync after merge; run `scripts/tighten-weak-ensures.py` or mirror sync scripts if copies still use `ensures true` on value returns.
