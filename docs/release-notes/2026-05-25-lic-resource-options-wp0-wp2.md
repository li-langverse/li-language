# CLI resource flags + parallel li-tests (WP0/WP2)

## Summary

`ResourceOptions` adds `--jobs`, `--max-memory`, `--job-memory-mb`, `--build-dir`, and `--threads` to `lic`; `run_all.sh` passes `--build-dir` per parallel worker instead of requiring `LI_BUILD_DIR`.

## Agent continuation

1. Read `compiler/common/include/li/resource_options.hpp`.
2. Run `./scripts/build.sh` and `./li-tests/tooling/resource_flags_smoke.sh`.
3. Next: WP1 advisory; WP3 workspace check; integrator `ci.sh -j8`.
4. Blocked: none.

## Changed

- `compiler/common/`, `compiler/lic/main.cpp`
- `li-tests/run_all.sh`, `li-tests/tooling/resource_flags_smoke.sh`, `run_all_parallel_smoke.sh`

## Not changed

- `compiler/analyze/`, check cache, native parallel runtime, toolchain benches
- `scripts/ci.sh` (Wave 2 integrator)
- Proof gates, `trusted.lean`, tier physics benches

## Breaking

N/A — env vars deprecated with one-time stderr warnings; flags win when both set.

## Security / Performance

N/A / subprocess pool isolates AutoVC per `run_all` worker (`--build-dir`).

## Downstream

WP1/WP3 consume `ResourceOptions`; WP4 uses `--threads`; WP5 toolchain rows reference `lic check --jobs`.
