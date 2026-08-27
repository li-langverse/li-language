# lic check workspace driver + incremental check cache (WP3)

## Summary

Adds `lic check --workspace` with a subprocess job pool and diagnostics-only LRU cache flags for single-file `lic check`.

## Agent continuation

1. Read `compiler/lic/workspace_check.cpp`, `compiler/cache/check_cache.cpp`, `compiler/lic/check_cmd.cpp`.
2. Run `chmod +x li-tests/tooling/check_workspace_cache_smoke.sh && ./li-tests/tooling/check_workspace_cache_smoke.sh` after `scripts/build.sh`.
3. Next: open stacked PR on `feat/lic-check-advisory` after WP1 (#212) merges; rebase onto `feat/lic-resource-options` when integrator merges #205.
4. Blocked: none for WP3 scope.

## Changed

- `compiler/lic/workspace_check.cpp` — member discovery from `packages/li.toml`, fork pool, `--jobs` / `--max-memory`.
- `compiler/cache/check_cache.cpp` — `.li/cache/check` JSON LRU (`--cache-dir`, `--cache-max-mb`, `--no-cache`).
- `compiler/lic/check_cmd.cpp` — cache in `lic_check_main`; `--workspace` dispatch.
- `compiler/lic/main.cpp` — usage + `lic_check_main(..., argv[0])`.
- `li-tests/tooling/check_workspace_cache_smoke.sh` — cold/warm + workspace smoke.

## Not changed

- `runtime/li_rt.c`, `emit.cpp`, `analyze/` rules, `run_all.sh`, `bench_toolchain/`.

## Breaking

N/A — new flags only; default behavior unchanged (`--jobs=1`, cache on unless `--no-cache`).

## Security

N/A — cache stores diagnostic JSON only; subprocess uses `execv` (no shell).

## Performance

- Workspace: subprocess per member; effective jobs capped by `--max-memory` / `--job-memory-mb` (128 MiB default for check).
- Cache: streaming FNV hash; LRU eviction by mtime.

## Downstream

- Stack PR base: `feat/lic-check-advisory` (WP1 #212).
