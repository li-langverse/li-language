# Li semantics (Lean 4)

This directory holds the **canonical mathematical definition** of Li Core and the **trusted axiom base**.

## Files

| File | Role |
|------|------|
| `trusted.lean` | **Only** unproved axioms (`IO`, extern hooks) — audited, minimal |
| `Core.lean` (planned) | Typing rules, contract semantics, `decreases` |
| `MIR.lean` (planned) | Preservation lemmas for lowering |

## Rule

User `.li` modules may **not** add axioms. If it is not provable from `Core` + lemmas, it does not compile.

## Building

```bash
lake build   # when Lake project is wired (Phase 2f)
```

## Related

- [Verification overview](../verification/overview.md)  
- [Language design spec](../superpowers/specs/2026-05-14-li-language-design.md)  
