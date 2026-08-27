# Phase 2j-a: method call syntax (`obj.method(args)`)

## Summary

Implements **2j-a** method-call sugar: `c.bump()` desugars to `Counter_bump(self, …)` via parser `MethodCall`, typecheck, and MIR `CallProc`; **150/150** `li-tests` green.

## Agent continuation

1. **Read** `docs/superpowers/plans/2026-05-20-li-oop-roadmap.md` § **2j-b** (private methods) and **2j-c** (write-back).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh`.
3. **Then** **2j-b** `private def` visibility + cross-module method tests; **2j-c** `var` object write-back after calls.
4. **Blocked on** httpd paths (`packages/li-http/**`, `li_rt_httpd.c`) — other agents own PRs **#84** / **#87**.

## Changed

| Path | What |
|------|------|
| `compiler/ast/include/li/ast.hpp` | `Expr::Kind::MethodCall` |
| `compiler/parser/parser.cpp` | Parse `recv.method(args)` after field chain |
| `compiler/types/typecheck.cpp` | Resolve `TypeName_method`, `self` + arg checks |
| `compiler/mir/lower.cpp` | Lower to `CallProc` with object receiver args |
| `compiler/types/borrowck.cpp` | Walk receiver/args on method calls |
| `li-tests/encapsulation/def_method_*.li` | verify_ok + missing-method compile_fail |
| `li-tests/manifest.toml` | Three new encapsulation rows |

## Not changed

- `packages/li-http/**`, `packages/li-net-httpd/**`, HTTP tier-5 benches.
- Private **methods**, inheritance, traits (**2j-b**–**f**).
- `var RigidBody` callee write-back after `integrate` (**2j-c**).
- Structured `disjoint=` AST (**7d-c**).

## Breaking

N/A — additive syntax; existing `Type_method(self, …)` calls unchanged.

## Security

N/A

## Performance

N/A — static desugar only; no vtables.

## Downstream

- Master plan **2j-a** exit gate satisfied; update tracker checkbox when PR merges.
