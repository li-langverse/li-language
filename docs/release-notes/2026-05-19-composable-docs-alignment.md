# Docs: composable imports and def syntax alignment

## Summary

Align composable/httpd documentation and package README with workspace import resolution (`net.httpd`, `physics.*`) and `def` procedure syntax.

## Agent continuation

1. Read `docs/ecosystem/composable-by-default.md` and `docs/language/import-style.md`.
2. When editing package READMEs, use `import_name` from each package `li.toml`.
3. Next: optional mirror sync for `li-httpd` if README should match org repo.
4. Blocked: none.

## Changed

- `docs/ecosystem/composable-by-default.md` — resolution order, `def`, `net.httpd`, physics composable tests.
- `docs/language/import-style.md` — resolver order, legacy import warnings.
- `packages/li-net-httpd/README.md` — `import net.httpd`, Li example syntax.

## Not changed

- Compiler import resolver (landed in #63).
- httpd implementation (still stubs).

## Breaking

N/A — documentation only.

## Security / Performance / Downstream

N/A.
