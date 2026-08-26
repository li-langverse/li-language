# Release notes: refinement types at calls and initializers

## Summary

Refinement types (`{x: int | …}` and aliases like `NonNeg`) are enforced at **call sites** and **`var` initializers**: provable violations emit **E0305**; unknown cases emit Lean VCs like `requires`.

## Agent continuation

1. **Read** `compiler/verify/call_requires.cpp` (`resolve_refinement_on_type`, `check_refinement_argument`), `typecheck.cpp` (`check_value_matches_refinement`).
2. **Run** `LI_ALLOW_OPEN_VC=1 LIC=./build/compiler/lic/lic bash li-tests/run_all.sh` and filter `refinement_`.
3. **Next** caller `requires` → callee discharge, `and` bounds static eval, return-type refinements.

## Changed

| Path | Change |
|------|--------|
| `compiler/verify/call_requires.{hpp,cpp}` | `ProofFacts`, if-guard `>= 0` discharge; resolve/check/explain refinements |
| `compiler/lic/main.cpp` | `run_lean_verify_after_build` when `LI_BUILD_VERIFY_LEAN=1` |
| `scripts/ci.sh` | `LI_BUILD_VERIFY_LEAN_STRICT=1` + autovc on CI sample |
| `compiler/types/typecheck.cpp` | E0305 on calls + `var` init |
| `compiler/verify/vc_emit_lean.cpp` | Call-site refinement VCs |
| `compiler/diagnostics/*` | **E0305** `type.refinement` |
| `li-tests/contracts_verify/refinement_*.li` | Positive + negative corpus |
| `docs/language/refinement-types.md` | User-facing refinement guide |

## Not changed

- Symbolic discharge from caller `requires` or `if` guards.
- Compound predicates (`0 <= i and i < 10`) static evaluation beyond literals.
- `lip` / benchmarks.

## Breaking

N/A — stricter checking only for code that already violated declared refinements.

## Security

N/A.

## Performance

Compile-time only.

## Downstream

None.
