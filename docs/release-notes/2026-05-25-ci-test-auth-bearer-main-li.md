# Release notes: 2026-05-25 — ci-test-auth-bearer-main-li

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** (fix/ci-test-auth-bearer)  
**PH / REQ:** Phase H (li-httpd M1)  
**Author:** agent

---

## Summary (one sentence)

`build/li-httpd` now links `packages/li-net-httpd/src/main.li` so CI `test-auth-bearer.sh` exercises a real CLI instead of a stub `main` that returned 0 immediately.

## Agent continuation (required)

1. Read: `scripts/build-li-httpd.sh`, `scripts/test-auth-bearer.sh`, `packages/li-net-httpd/src/main.li`.
2. Run: `./scripts/build-li-httpd.sh && ./scripts/test-auth-bearer.sh` (expect `test-auth-bearer: ok`).
3. Then: merge this PR before rebasing #184–#186; confirm GHA `build-and-test` / `build-and-test-macos` green.
4. Blocked on: **none**

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Build | `build-li-httpd.sh` compiles `main.li` (not `lib.li` alone) | Local `./scripts/test-auth-bearer.sh` → 401/401/200 |
| CI | Removed `HTTPD_SKIP_AUTH_BEARER_SMOKE=1` from ubuntu + macos jobs | `.github/workflows/ci.yml` |

- `scripts/build-li-httpd.sh` — entry comment + `main.li` output path unchanged (`build/li-httpd`).

## Not changed (scope fence)

- `runtime/li_rt_net.c` epoll / blocking serve implementation — **not** modified.
- `HTTPD_SKIP_AUTH_BEARER_SMOKE` in `scripts/local-ci.sh` Docker path — **unchanged** (container TCP smoke still skipped per `docs/ecosystem/local-ci-docker-images.md`).
- PRs #184–#186 branch content — **not** merged here; only unblocks their CI after merge.

## Breaking changes

None.

## Security

N/A — bearer gate behavior unchanged; smoke test now runs against the intended binary.

## Performance

N/A.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |

## CHANGELOG entry (paste into Unreleased)

```markdown
### Fixed
- **CI `test-auth-bearer`:** `build-li-httpd.sh` links `main.li` so `li-httpd` runs `httpd_run_from_argv` (was stub `main` returning 0) — `docs/release-notes/2026-05-25-ci-test-auth-bearer-main-li.md`.
```
