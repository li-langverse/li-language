# 2026-08-17 — Decorator typosquat check covers modern `@` syntax; `lic check` shows warnings on success

## Summary

Three related fixes landed together. The typosquat/reserved-name decorator policy
(`compiler/types/policy.cpp`) was written against the spec'd-but-never-parsed
`decorator def NAME(...)` syntax; the parser moved to `@name` decorators long ago,
so the W0403 typosquat check was dead code for real programs — `@my_paralell` (a
misspelling of `@parallel`) compiled silently. In addition, `lic check` in human
mode only printed diagnostics on failure, hiding advisory warnings behind a green
exit code, and `resolve_imports` returned `diags.empty()` so any pre-existing
warning failed the whole frontend.

## Changes

| Area | What | Evidence |
|------|------|----------|
| `compiler/types/policy.cpp` | `check_decorator_policies` now also scans modern `@name` decorator applications (line-start `@` + identifier) for typosquats; the legacy `decorator def` scan remains for the definition-form reserved/prefix/segment checks | `li-tests/decorator_exploits/typosquat_paralell.li` now warns `W0403` and passes `check_ok` |
| `compiler/types/import_resolve.cpp` | `resolve_imports` returns `!diags.has_errors()` instead of `diags.empty()`, so warnings from earlier frontend passes cannot fail import resolution | `lic check` on a warning-only file exits 0 |
| `compiler/lic/check_cmd.cpp`, `compiler/diagnostics/` | Human-mode `lic check` renders and caches warnings/notes on success (`render_diagnostics`); exit code remains 0 unless `--deny-warnings` escalates | `lic check` prints `W0403` with rc=0; `--deny-warnings` still exits 1 |
| `li-tests/decorator_exploits/typosquat_paralell.li`, `li-tests/advisory/deny/check_fail.li` | Modernized from legacy `decorator def` to `@my_paralell` on a real `def`; the deny-config test still asserts `E0330` | `decorator_exploits` 4/4, `advisory` 3/3 |
| Docs | `docs/language/decorators.md` and `docs/testing/overview.md` updated to describe the `@`-syntax typosquat check | — |

## Security

- Closes a silent-typosquat gap: a misspelled execution decorator (`@my_paralell`
  vs `@parallel`) is now flagged (W0403 warn, or E0330 under `[check].typosquat = "deny"`)
  instead of compiling silently and dropping the intended placement/parallelism.
- `--deny-warnings` behavior is unchanged and still fails CI-style runs.

## Testing

- `decorator_exploits` 4/4, `advisory` 3/3, `decorators` 9/9.
- Regression sweep on suites touched by the frontend/import changes: `stdlib_seal` 6/6,
  `encapsulation` 26/26, `lexer_parser` 7/7, `nanoreactor` 7/7, `smoke` 5/5, `runtime` 4/4.
- Tooling gates: `diagnose_json_smoke`, `check_workspace_cache_smoke`,
  `check_pkg_workspace`, `agent_manifest_smoke`, `ci_test_jobs_smoke` all ok.
