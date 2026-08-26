# Li semantics (Lean 4)

This directory holds the **canonical mathematical definition** of Li Core and the **trusted axiom base**.

## Files

| File | Role |
|------|------|
| `trusted.lean` | **Only** unproved axioms (`IO`, extern hooks) — audited, minimal |
| `Core.lean` | Phase **2f** stub (`core_stub_ok`); full rules planned |
| `MIR.lean` (planned) | Preservation lemmas for lowering |
| [`proof-db/`](../../proof-db/README.md) | Standard lemma registry + `ProofDB.lean` |

## Rule

User `.li` modules may **not** add axioms. If it is not provable from `Core` + lemmas, it does not compile.

**Today:** `Core.lean` is a **stub**; every `lic build` writes `build/generated/AutoVC.lean` (typed contract Props). Default `lic build` runs `lake build AutoVC` when Lean 4 is installed (`--no-lean-verify` to skip). Kernel discharge of all ensures is not yet wired. See **[Provability gaps](../verification/provability-gaps.md)** (**G-lean**, **G-trust**, **G-vc**).

## Building

```bash
lake build   # when Lake project is wired (Phase 2f)
```

## Related

- [Verification overview](../verification/overview.md)  
- [Language design spec](../superpowers/specs/2026-05-14-li-language-design.md)  
