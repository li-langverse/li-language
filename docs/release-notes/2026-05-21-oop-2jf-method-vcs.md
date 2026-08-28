# Phase 2j-f — Method call-site requires + AutoVC

## Summary

Method calls (`obj.method(args)`) now check desugared `Type_method` callee `requires` at compile time (E0304) and emit Lean AutoVC call-site obligations like ordinary `Call`.

## Agent continuation

1. **Read** master plan **2i** / **7e** (matrix `@`, math→SIMD) or close **2j** PR stack (#136 + this branch).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh` (expect **159** pass).
3. **Optional:** extend `old(self.field)` ensures sugar (spec only today).
4. **Blocked on** httpd packages.

## Changed

| Path | What |
|------|------|
| `compiler/verify/call_requires.cpp` | `check_requires_at_method_call`, object field const folding (`w.balance`) |
| `compiler/types/typecheck.cpp` | E0304 on violated method `requires` |
| `compiler/verify/vc_emit_lean.cpp` | AutoVC for method call sites |
| `li-tests/contracts_verify/method_call_requires_*.li` | pass/fail contract |

## Not changed

- `old(self.field)` / method `ensures` sugar (spec deferred).
- `invariant` on `var self` in method loops.
- Dynamic dispatch / vtables.

## Breaking

N/A.

## Security

Strengthens — method preconditions enforced like free `def` calls.

## Performance

N/A — compile-time only.

## Downstream

- Proof corpus row `method_call_requires_ok.li` in [proof-corpus-roadmap.md](../verification/proof-corpus-roadmap.md).
