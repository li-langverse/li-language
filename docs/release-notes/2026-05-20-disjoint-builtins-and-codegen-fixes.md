# Disjoint proof builtins + void-call / float-arg codegen fixes

## Summary

Registers `disjoint_elem`, `disjoint_row`, `disjoint_slice`, and `row_ok` as compiler proof builtins; fixes `CallProc` codegen for `-> unit` calls and float literal args; restores generic return checking and `li-tests` green (146/146).

## Agent continuation

1. **Read** `compiler/types/typecheck.cpp` (disjoint builtins, generic return check, `str`→`ptr` for `extern`), `compiler/codegen/emit.cpp` (`CallProc` void/float coercion), `compiler/mir/lower.cpp` (`FloatLit` call args).
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh` (expect 146 pass); `./li-tests/run_httpd_config.sh` if touching httpd (unchanged here).
3. **Then** Phase **7d-c**: structured `disjoint=` on AST (replace `policy.cpp` string heuristics); optional `std/execution/disjoint.li` stubs for docs only.
4. **Blocked on** httpd package PRs **#84** / **#87** — do not edit `packages/li-http/**`, `packages/li-net-httpd/**`, or `runtime/li_rt_httpd.c` serve/epoll paths in parallel.

## Changed

| Path | What |
|------|------|
| `compiler/types/typecheck.cpp` | Builtin `disjoint_*` / `row_ok` → `bool`; generic return **E0202**; `str` actuals for `ptr`/`extern` params |
| `compiler/types/prelude.cpp` | Reserve disjoint proof names (E0330 shadow) |
| `compiler/codegen/emit.cpp` | Skip store on void `CallProc`; float literal + f32/i32 arg coercion |
| `compiler/mir/lower.cpp` | `FloatLit` in `CallProc` args; no dest temp for `-> unit` calls |
| `compiler/mir/include/li/mir.hpp` | `MirArg.is_float_literal` |
| `li-tests/generics/return_mismatch.li` | `ensures result == 0` so generic mismatch surfaces |

## Not changed

- `packages/li-http/**`, `packages/li-net-httpd/**`, `runtime/li_rt_httpd.c` (other agent).
- Lean kernel discharge (**G-lean**) or removal of `policy.cpp` disjoint string scans (**7d-c** full).
- `var RigidBody` write-back across `CallProc` (still by-value scalar expansion).

## Breaking

N/A — fixes latent codegen/typecheck gaps; parallel/disjoint tests that already compiled keep behavior.

## Security

N/A — proof builtins are compile-time hooks only (no runtime trust change).

## Performance

N/A — no hot-path change; `li-tests/run_all.sh` full manifest ~146 tests.

## Downstream

- Composable `import_physics_runtime.li` now **builds** (was SIGSEGV on void `rigid_integrate_semi_implicit` call).
- Bootstrap `bootstrap/lic/main.li` builds with `strcmp(li_rt_argv(1), "--version")`.
