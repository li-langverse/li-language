# Phase 2j-d — Nominal inheritance (static subtyping)

## Summary

Adds `type Derived = object of Base`, flattens base fields in typecheck/MIR, allows **Derived** where **Base** is expected, and checks `@override` method signatures against the base type.

## Agent continuation

1. **Read** OOP roadmap § **2j-e** (traits).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh` (expect **155** pass).
3. **Then** implement `trait_hash_impl.li` / `trait_missing_impl.li` on branch `cursor/oop-2je-traits-57b4`.
4. **Blocked on** httpd — do not touch `packages/li-http/**`.

## Changed

| Path | What |
|------|------|
| `compiler/ast/include/li/ast.hpp` | `TypeAlias.base_object` |
| `compiler/parser/parser.cpp` | `object of Base` syntax |
| `compiler/types/typecheck.cpp` | Flatten layout, subtype assign, `@override` check |
| `compiler/mir/lower.cpp` | `for_each_object_field` for inherited slots |
| `li-tests/encapsulation/inheritance_*.li`, `override_mismatch.li` | pass/fail contract |

## Not changed

- Virtual dispatch / vtables (static only).
- **2j-e/f** traits and method Lean VCs.
- Httpd packages.

## Breaking

N/A — new syntax only; existing object types unchanged.

## Security

N/A — layout mismatch rejected at compile time.

## Performance

N/A — extra MIR slots for derived fields only.

## Downstream

- Physics packages may introduce `object of` shared bases when modeling entity hierarchies.
