# CallProc string args + two-pass LLVM emit

## Summary

`lic build` now emits calls to imported `.li` procedures with string literal arguments and fixes declaration order so `main` can call helpers defined later in the MIR list (e.g. `match_route_fixture`).

## Agent continuation

1. **Read** `compiler/codegen/emit.cpp` (two-pass declare/emit), `compiler/mir/lower.cpp` (`StringLit` in `CallProc` args), `li-tests/run_httpd_config.sh`.
2. **Run** `LI_ALLOW_OPEN_VC=1 LI_REPO_ROOT=$PWD ./build/compiler/lic/lic build li-tests/routing/match_routes.li -o /tmp/li_match_routes && /tmp/li_match_routes` (expect exit 0); `./li-tests/run_httpd_config.sh`.
3. **Then** Phase H: TOML route loader in Li (replace hardcoded `li_rt_match_route_fixture` table); wire `li-net-httpd` to `http.match_route_fixture`.
4. **Blocked on** human merge of PR #83 if CI red on unrelated `li-tests` flakes; do not force-push.

## Changed

| Path | What |
|------|------|
| `compiler/codegen/emit.cpp` | Pass 1: declare all MIR fns; pass 2: bodies; `CallProc`/`CallExtern` string args → `i8*`; string params in `ptr_locals` |
| `compiler/mir/lower.cpp` | `StringLit` → `MirArg{is_string}` for `CallProc`; `bytes` params use `i8*` (`mir_ptr_param_type_name`) |
| `li-tests/routing/match_routes.li` | Single `main` oracle vs `api_prefix.toml` |
| `li-tests/run_httpd_config.sh` | Run compiled `/tmp/li_match_routes`, assert exit 0 |

## Not changed

- Lean `AutoVC` discharge rules or refinement typing semantics.
- `li_rt_match_route_fixture` C table (still fixture-only; not TOML-driven).
- `lic` parser, import resolve, or benchmark harness ingest.
- Org merge automation labels (`merge-approved` still required).

## Breaking

N/A — codegen fix only; no source-level API change.

## Security

N/A — no trust boundary change; same `extern` runtime symbols.

## Performance

N/A — one extra LLVM declare pass at compile time; no runtime hot-path change.

## Downstream

N/A — branch stacked on #83; pins unchanged until merge to `main`.
