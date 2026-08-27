# G-oop: method call-site requires and ensures corpus

## Summary

Strengthens **G-oop** with Lean AutoVC for folded method call-site `requires` and a method `ensures` specimen on int return; retags manifest rows to `prove_lean_ok`.

## Agent continuation

1. Read `docs/verification/provability-gaps.md` (**G-oop**) and `docs/verification/proof-corpus-roadmap.md` (**P-oop**).
2. Run `./scripts/build.sh`, `./li-tests/tooling/discharge_method_call_requires_lean.sh`, `./li-tests/tooling/discharge_method_ensures_return_lean.sh`, `./li-tests/run_all.sh contracts_verify`.
3. Next: trait dispatch call-site `requires`; `old(self.field)` in Lean.
4. **#185** (G-test-verify) merged; **#186** closed — duplicate of **#200** (8p-a).

## Changed

| Path | Evidence |
|------|----------|
| `compiler/verify/vc_emit_lean.cpp` | Folded call-site `requires` Props |
| `li-tests/contracts_verify/method_ensures_return_ok.li` | Method `ensures result == 0` |
| `li-tests/tooling/discharge_method_*_lean.sh` | Zero open AutoVC |
| `li-tests/manifest.toml` | `prove_lean_ok` for method specimens |
| `docs/verification/provability-gaps.md`, `proof-corpus-roadmap.md` | **G-oop** / **P-oop** |

## Not changed

Trait dispatch VCs, `old(self.field)`, virtual dispatch.

## Breaking / Security / Performance / Downstream

N/A.
