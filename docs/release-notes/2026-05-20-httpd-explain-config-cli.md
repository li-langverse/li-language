# Release notes: `lic httpd explain-config` + httpd verify_open_ok

## Summary

Adds **`lic httpd explain-config <file.toml>`** (delegates to `scripts/httpd_config.py --explain`), C/Python golden parity via **`check-httpd-explain-config.sh`**, and marks httpd/routing composable manifest tests **`verify_open_ok`** so CI matches `LI_ALLOW_OPEN_VC=1` policy for trusted http externs.

## Agent continuation

1. Read `compiler/lic/main.cpp` (`httpd_explain_config`), `scripts/check-httpd-explain-config.sh`, `runtime/li_rt_httpd.c` (`li_rt_httpd_explain_config`).
2. Run `./li-tests/run_httpd_config.sh` and `./li-tests/run_all.sh routing httpd composable`.
3. Then: surface config errors as `lic` diagnostics; pure-Li explain without Python.
4. Blocked on: argv-as-`str` for `lic httpd explain-config` without `LI_REPO_ROOT`.

## Changed

| Path | Change |
|------|--------|
| `compiler/lic/main.cpp` | `lic httpd explain-config` subcommand |
| `runtime/li_rt_httpd.c` | `li_rt_httpd_explain_config` |
| `scripts/li-httpd-explain-config.sh`, `check-httpd-explain-config.sh`, `httpd_explain_main.c` | Golden parity |
| `li-tests/config_desugar/good/agent_gateway.explained.golden` | Expected explain output |
| `li-tests/manifest.toml` | routing + httpd + composable http → `verify_open_ok` |
| `li-tests/run_httpd_config.sh`, `scripts/ci.sh` | explain-config gate |

## Not changed

- Full `[server]` / `[limits]` TOML in Li.
- li-httpd serve loop or TLS.
- Default `lic build` strict open-VC policy (only manifest httpd tests use `verify_open_ok`).

## Breaking

N/A

## Security

Read-only config read in `li_rt_httpd_explain_config`; audit row in `trusted-c-audit.toml`.

## Performance

N/A

## Downstream

Agents debugging routes: `lic httpd explain-config path/to.toml` before editing `routing.toml`.
