# Language naming conventions (PascalCase types)

## Summary

Adds `docs/language/naming-conventions.md` requiring **PascalCase** (`ClassName`) for type/object names and **snake_case** for `def`, variables, and fields; wires handbook, mkdocs, and agent rule.

## Agent continuation

1. **Read** `docs/language/naming-conventions.md` before naming new `type … = object` or public APIs.
2. **Run** N/A (docs-only); optional `mkdocs build` if editing site nav.
3. **Then** opportunistically align legacy snake_case type names in `packages/*` when touching those modules.
4. **Blocked on** N/A.

## Changed

| Path | What |
|------|------|
| `docs/language/naming-conventions.md` | New canonical casing table |
| `docs/language/philosophy.md`, `overview.md`, `types-and-data.md` | Cross-links + PascalCase emphasis |
| `.cursor/rules/li-readability-philosophy.mdc` | Agent naming table |
| `mkdocs.yml` | Handbook nav entry |

## Not changed

- Compiler enforcement of naming (style is guideline, not a diagnostic yet).
- Existing type identifiers in physics/http packages (no mass rename).

## Breaking

N/A — documentation only.

## Security

N/A

## Performance

N/A

## Downstream

N/A
