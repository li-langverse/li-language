# Check advisories and severity (WP1)

## Summary

`lic check` runs advisory passes after typecheck (unreachable code, ensures notes); warnings and notes use diagnostic JSON severities; `--deny-warnings` fails CI-style runs; `li.toml` `[check].typosquat` controls decorator typosquat severity.

## Agent continuation

1. Read `compiler/lic/check_cmd.cpp` and `compiler/analyze/advisory.cpp`.
2. Run `./scripts/build.sh` and `lic check li-tests/advisory/unreachable_after_return.li`.
3. Next: WP3 workspace driver rebased on this branch; merge after WP0 (#205).
4. Blocked: none.

## Changed

- `compiler/analyze/`, `compiler/config/check_config.cpp`, `compiler/lic/check_cmd.cpp`
- `compiler/diagnostics/` severities; `compiler/types/policy.cpp` typosquat warn path
- `li-tests/advisory/unreachable_after_return.li`

## Not changed

- `run_all.sh`, check cache, native runtime, benches
- Proof / Lean gates on `lic build`

## Breaking

N/A — default typosquat is warn via `CheckConfig`; deny still available in `li.toml`.

## Security / Performance

N/A / AST-only advisory walks after typecheck.

## Downstream

WP3: `run_frontend_check` API in `check_cmd.hpp`.
