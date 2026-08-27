# Release notes: call-site requires — typecheck, import, locals, build gate

## Summary

Call-site callee `requires` now **fail `lic build` on open Lean goals** (opt-out `LI_ALLOW_OPEN_VC=1`), **reject provably false calls** (**E0304**), discharge **const locals** (`var y = 5` → `callee(y)`), and emit VCs for **`extern`** callees and **imported** defs.

## Agent continuation

1. **Read** `compiler/verify/call_requires.cpp`, `compiler/types/typecheck.cpp` (`check_call_args`), `compiler/lic/main.cpp` (open VC gate), `.cursor/rules/li-def-not-proc.mdc`.
2. **Run** `./li-tests/run_all.sh contracts_verify modules && ./li-tests/tooling/contracts_verify_lean.sh`.
3. **Then** extend non-literal flow (symbolic ranges); wire `LI_BUILD_VERIFY_LEAN_STRICT` + lake on every build when Lean is installed.
4. **Blocked on** proving float `abs` postconditions (`sqrt_open_bound.li` uses manifest outcome `verify_open_ok`).

## Changed

| Path | Change |
|------|--------|
| `compiler/verify/call_requires.{hpp,cpp}` | Shared substitute/fold/check; **`explain_requires_violation`** plain-language **E0304** |
| `compiler/verify/vc_emit_lean.cpp` | Call-site VCs for all resolved callees including `extern` |
| `compiler/types/typecheck.cpp` | **E0304** on violated requires; const-int local map; unknown call error |
| `compiler/lic/main.cpp` | Default fail build when `AutoVC` has open goals |
| `compiler/diagnostics/*` | **E0304** `contract.callee_requires` |
| `li-tests/contracts_verify/*` | ok/fail/local/extern tests; `sqrt_open_bound` → `verify_open_ok` |
| `li-tests/modules/positive/`, `use_positive.li` | Import + call-site discharge |
| `li-tests/run_all.sh` | `verify_open_ok` outcome |
| `.cursor/rules/li-def-not-proc.mdc` | **`def` not `proc`** (always apply) |

## Not changed

- Full Lean kernel on every `lic build` (still `LI_BUILD_VERIFY_LEAN` opt-in).
- Symbolic non-constant locals without literal init.
- **`lip` / `lit` / benchmarks** repos.

## Breaking

| Item | Migration |
|------|-----------|
| `lic build` fails on open `AutoVC` goals | Discharge proofs or `LI_ALLOW_OPEN_VC=1` (emergency); specimens with intentional open VCs use `verify_open_ok` in manifest |
| **E0304** on bad call args | Read the spelled-out precondition (e.g. ``Cannot call `callee(-1)`: `callee` requires `x >= 0`…``) |

## Security

N/A — static proof gate only.

## Performance

N/A — compile-time only.

## Downstream

None — `lic` monorepo.
