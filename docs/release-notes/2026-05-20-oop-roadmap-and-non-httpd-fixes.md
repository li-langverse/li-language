# Phase 2j OOP roadmap + non-httpd compiler fixes

## Summary

Adds the **full OOP roadmap** (Phase **2j**), **G-oop** provability gap, PascalCase naming guide, disjoint proof builtins, and **CallProc** codegen fixes; **147/147** `li-tests` pass including cross-module private field rejection.

## Agent continuation

1. **Read** `docs/superpowers/plans/2026-05-20-li-oop-roadmap.md` before implementing methods/`self` (**2j-a**).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh`.
3. **Then** **2j-a** parser/`self` desugar — not httpd packages; parallel **7d-c** AST disjoint optional.
4. **Blocked on** httpd PRs **#84** / **#87** for `packages/li-http/**` / `li_rt_httpd.c` serve paths.

## Changed

| Path | What |
|------|------|
| `docs/superpowers/plans/2026-05-20-li-oop-roadmap.md` | New Phase 2j milestones 2j-a…f |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Phase 2j row + tracker + v2 backlog |
| `docs/verification/provability-gaps.md` | **G-oop** row |
| `docs/language/naming-conventions.md` | PascalCase `ClassName` for types |
| `compiler/types/typecheck.cpp`, `prelude.cpp` | Disjoint builtins; generic return; str→ptr |
| `compiler/codegen/emit.cpp`, `compiler/mir/*` | Void `CallProc`, float literal args |
| `li-tests/encapsulation/leak_imported_vault.li` | Cross-module private field compile_fail |

## Not changed

- `packages/li-http/**`, `packages/li-net-httpd/**`, httpd runtime serve/epoll.
- Methods/`self`, traits, inheritance implementation (**2j** not started).
- `var RigidBody` callee write-back (2j-c).

## Breaking

N/A

## Security

N/A — stronger encapsulation test coverage for imported types.

## Performance

N/A

## Downstream

- Supersedes open items from disjoint/CI branch (PR #131) when merged.
- Naming guidelines PR #132 can close in favor of this combined branch or rebase.
