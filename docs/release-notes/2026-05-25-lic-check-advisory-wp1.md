# Release notes: lic check advisory WP1

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** feat/lic-check-advisory  
**PH / REQ:** WP1 check/diagnose/advisory  
**Author:** agent

---

## Summary (one sentence)

Implements WP1 end-to-end by wiring configurable check policy (`li.toml` `[check].typosquat`), advisory diagnostics (W0401/W0402/W0403, N0401), and a shared `run_frontend_check` path used by `lic check`, `lic diagnose`, and `frontend()` in `lic main`.

## Agent continuation (required)

1. Read: `compiler/lic/include/li/check_cmd.hpp`, `compiler/lic/check_cmd.cpp`, `compiler/analyze/include/li/advisory.hpp`, `compiler/analyze/advisory.cpp`.
2. Run: `./scripts/build.sh`, `./li-tests/run_all.sh advisory`, `./li-tests/tooling/diagnose_json_smoke.sh`.
3. Then: in WP3, reuse `run_frontend_check` for workspace-scale check orchestration (workspace walk/cache layering) without changing this per-file frontend contract.
4. Blocked on: `origin/feat/lic-resource-options` is not on remote; WP1 proceeded with current `main` resource-flag behavior and did not require additional `ResourceOptions` changes.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `compiler/diagnostics/` | Severity-aware diagnostics (`error`/`warning`/`note`), `has_errors`/`has_warnings`, JSON `ok` depends on errors only | `diagnostics.cpp`, `error_codes.cpp` |
| `compiler/config/` | Added check config loader for nearest `li.toml` `[check].typosquat = warn|deny` | `check_config.cpp`, `check_config.hpp` |
| `compiler/analyze/` | Advisory passes for unused import (W0401), unreachable statement (W0402), requires-without-ensures note (N0401) | `advisory.cpp` |
| `compiler/types/policy.*` | Typosquat policy now configurable: W0403 warn by default, E0330 deny when configured | `policy.cpp`, `policy.hpp` |
| `compiler/lic/` | Added `run_frontend_check` API + `lic_check_main`/`lic_diagnose_main`; `--deny-warnings`; `main.cpp` dispatch delegates to command handlers | `check_cmd.cpp`, `check_cmd.hpp`, `main.cpp` |
| CMake wiring | Added/linked `li_config` and `li_analyze` where needed (`compiler`, `types`, `lic`) | `compiler/CMakeLists.txt`, `compiler/types/CMakeLists.txt`, `compiler/lic/CMakeLists.txt` |
| `li-tests/` | Added advisory fixtures/outcomes (`check_ok`, `check_deny_warn`, `check_fail`), migrated `typosquat_paralell` to warning path, expanded JSON smoke for mixed severity | `manifest.toml`, `run_all.sh`, `advisory/*`, `tooling/diagnose_json_smoke.sh` |

## Not changed (scope fence)

- No `runtime/` edits.
- No `emit.cpp` / `fopenmp` changes.
- No `workspace_check` or `check_cache` implementation (reserved for WP3).

## Breaking changes

None.

## Security

N/A — diagnostics/config/advisory behavior only; no new privileged runtime surface.

## Performance

N/A — no runtime/codegen hot-path changes; advisory pass runs in `lic check` frontend only.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |
