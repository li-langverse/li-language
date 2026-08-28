# Phase 2j-e — Traits and generic bounds

## Summary

Adds `type Hash = trait` with required method signatures, `def f[T: Hash]` generic bounds, and compile-time checks that object types implement traits via `Type_method` procedures.

## Agent continuation

1. **Read** OOP roadmap § **2j-f** (method Lean VCs).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh` (expect **157** pass).
3. **Then** extend AutoVC for desugared method calls; or **2i** matrix `@` if perf priority.
4. **Blocked on** httpd packages.

## Changed

| Path | What |
|------|------|
| `compiler/ast/include/li/ast.hpp` | `AliasKind::Trait`, `type_param_bounds`, `trait_methods` |
| `compiler/parser/parser.cpp` | `trait` type alias; `T: Hash` in type params |
| `compiler/types/typecheck.cpp` | Trait satisfaction; generic bound check at calls |
| `li-tests/encapsulation/trait_hash_impl.li` | verify_ok |
| `li-tests/encapsulation/trait_missing_impl.li` | compile_fail |

## Not changed

- Trait vtable / dynamic dispatch (static only).
- `dict[K,V]` requiring `K: Hash` in collections typechecker (stub ok per roadmap).
- **2j-f** method Lean VCs.

## Breaking

N/A — new syntax.

## Security

N/A — rejects missing trait impl at compile time.

## Performance

N/A — trait checks are compile-time only.

## Downstream

- Collections phase can wire `dict[K,V]` to `K: Hash` bound checking.
