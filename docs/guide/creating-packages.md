# Creating packages

This guide shows how to start a new Li package that matches the ecosystem layout used by **`lip`** (planned) and the **[`li-langverse`](https://github.com/li-langverse)** org.

## Quick start

From the [`li-language`](https://github.com/li-langverse/li-language) repository:

```bash
./scripts/build.sh
./scripts/li-new-package my-math --kind library
./scripts/li-new-package my-tool --kind binary
```

For an **official** package (org repo + `PKG-*` id):

```bash
./scripts/li-new-package li-demo --kind library --official
# Follow printed gh repo create instructions
```

## What gets created

```text
packages/my-math/
  li.toml              # manifest (canonical schema: lip plan § A3)
  README.md
  CHANGELOG.md         # Keep a Changelog
  SECURITY.md
  PUBLISH.md           # PKG id + publish metadata
  docs/traceability.md
  src/lib.li           # or src/main.li for --kind binary
  li-tests/smoke/builds.li
```

## Build and check

```bash
export LIC=./build/compiler/lic/lic
lic check packages/my-math/li-tests/smoke/builds.li
lic build packages/my-math/src/lib.li -o my-math
```

## `li.toml` essentials

See [package layout reference](package-layout-reference.md) and the [package manager plan](../superpowers/plans/2026-05-16-li-package-manager-lip.md).

Official packages set:

```toml
[package.metadata.li]
import_name = "math.foo"   # ergonomic: import math.foo

[package.metadata.lip]
maintainer = "li-langverse"
pkg_id = "PKG-my-math"

[package.repository]
url = "https://github.com/li-langverse/my-math"
```

See [import-style.md](../language/import-style.md). Physics packages use `physics.<area>` (e.g. `physics.relativity`).

## Workspace (monorepo)

```bash
mkdir -p packages
./scripts/li-new-package li-foo --workspace packages
```

This updates `packages/li.toml` `[workspace].members`.

## Publishing (later)

When **`lip`** ships:

- `lip init` will wrap the same scaffold
- `lip publish` requires `lic build`, signatures, and `lit` coverage ≥80%

Until then, push to **`li-langverse/<name>`** and list the package in [official-packages.md](../ecosystem/official-packages.md).

## Agents

Use the project skill `.cursor/skills/create-li-package/SKILL.md` — always run `li-new-package`, never invent directory layouts.

## Related

- [Ecosystem overview](../ecosystem/overview.md)
- [Governance](../ecosystem/governance.md)
- [Documentation style](../contributing/documentation.md)
