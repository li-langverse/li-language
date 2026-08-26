# Package layout reference

The **authoritative** `li.toml` schema is [Package manager plan § A3](../superpowers/plans/2026-05-16-li-package-manager-lip.md). Scaffold templates in `scripts/templates/package/` are generated from that schema.

## `[package]`

| Field | Required | Notes |
|-------|----------|-------|
| `name` | yes | kebab-case; registry name |
| `version` | yes | SemVer |
| `edition` | yes | Language / proof ABI (`2026`) |
| `license` | yes | SPDX expression, e.g. `Apache-2.0 OR MIT` |
| `description` | recommended | One line |

## `[dependencies]`

| Form | When |
|------|------|
| `name = "1.2.0"` | Registry (lip 8d) |
| `name = { git = "...", tag = "v1" }` | Git (lip 8b) |
| `name = { path = "../other" }` | Monorepo path (lip 8b) |

## `[[bin]]`

Required for binaries (`--kind binary`):

```toml
[[bin]]
name = "my-tool"
path = "src/main.li"
```

## `[package.metadata.lip]`

| Field | Official? |
|-------|-----------|
| `min_lic` | recommended |
| `min_coverage` | default 80 at publish |
| `maintainer` | **required** for `li-langverse` packages |
| `pkg_id` | **required** for official (`PKG-*`) |

## `[package.repository]`

| Field | Purpose |
|-------|---------|
| `url` | `https://github.com/li-langverse/<name>` |
| `documentation` | Docs site deep link |
| `changelog` | `CHANGELOG.md` on default branch |

## Files (official packages)

| File | Standard |
|------|----------|
| `CHANGELOG.md` | [Keep a Changelog](https://keepachangelog.com/) |
| `LICENSE` | SPDX license text |
| `SECURITY.md` | Reporting process |
| `PUBLISH.md` | `PKG-*`, exports, proof tier |
| `docs/traceability.md` | REQ/PH/T links |
