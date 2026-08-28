# HTTPd plan loop rebased on main (#163/#164)

## Summary

Rebase `cursor/httpd-plan-loop-54aa` onto `main` after proof CLI + mat2 discharge merges; `run_httpd_config.sh` uses `--allow-open-vc` and optional `HTTPD_SKIP_LI_ROUTING_BIN`.

## Agent continuation

1. **Merge** PR #162 after CI green.
2. **Run:** `HTTPD_SKIP_LI_ROUTING_BIN=1 ./li-tests/run_httpd_config.sh` in CI sandboxes with port conflicts; full run locally without skip.
3. **Next:** P-float `sqrt_open_bound`; 2i broadcast.

## Changed

| Area | Path |
|------|------|
| Merge | `li-tests/run_httpd_config.sh`, `CHANGELOG.md` |
| HTTPd | plan loop scripts + E0311 fixes from #162 branch |

## Not changed

- `sqrt_open_bound.li` proof.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A |
| **Security** | N/A |
| **Performance** | N/A |
| **Downstream** | N/A |
