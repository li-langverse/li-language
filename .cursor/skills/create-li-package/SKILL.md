---
name: create-li-package
description: >-
  Scaffold a new Li package with scripts/li-new-package under li-langverse
  governance. Use when the user asks to create a package, library, crate,
  li-new-package, lip init, or official li-langverse repo.
---

# Create Li package

## Rules

0. Read [engineering-standards.md](../../../docs/ecosystem/engineering-standards.md) and [vision-and-roadmap.md](../../../docs/ecosystem/vision-and-roadmap.md) first.
1. **Always** run `./scripts/li-new-package` from the `li-language` repo root — never hand-create `packages/` trees.
2. Use **`--official`** for `li-langverse` standard/first-party packages; follow printed `gh repo create` steps.
3. **`li.toml`** must match [package manager plan § A3](../../../docs/superpowers/plans/2026-05-16-li-package-manager-lip.md).
4. Every `proc` needs `requires`, `ensures`, `decreases` (no `sorry`, no `Any`).
5. Add **`PKG-*`** to [official-packages.md](../../../docs/ecosystem/official-packages.md) when `--official`.
6. **`min_coverage`:** default **80** in `li.toml`; **`li-std-*` / in-tree `std/`** slices use **100** (see engineering-standards).
7. **Composable API:** `src/lib.li` with lifecycle `proc`s; `src/main.li` demo-only — [composable-by-default.md](../../../docs/ecosystem/composable-by-default.md) + `composable-li-library` skill.

## Commands

```bash
# Library (default)
./scripts/li-new-package <kebab-name> --kind library

# Binary with main()
./scripts/li-new-package <kebab-name> --kind binary

# Official org package
./scripts/li-new-package <kebab-name> --kind library --official

# Monorepo member
./scripts/li-new-package <kebab-name> --workspace packages
```

## After scaffold

```bash
./scripts/build.sh
export LIC=./build/compiler/lic/lic
lic check packages/<name>/li-tests/smoke/builds.li
```

Register tests in root `li-tests/manifest.toml` if the package is part of CI.

## Org repo (official)

```bash
gh repo create li-langverse/<name> --public --license apache-2.0
cd packages/<name>   # or push standalone clone
git init && git remote add origin git@github.com:li-langverse/<name>.git
```

Copy `.github/PULL_REQUEST_TEMPLATE.md` from `scripts/templates/github-repo/` if missing.

**Mirror to org** (after local `lic check`):

```bash
set -a && source ../.env.github && set +a
./scripts/push-official-package-repo.sh <name> --create
```

Register in `.github/li-downstream-repos.txt` and [active-agent-claims.md](../../../docs/ecosystem/active-agent-claims.md).

## Docs to point users to

- [Engineering standards](../../../docs/ecosystem/engineering-standards.md)
- [Creating packages](../../../docs/guide/creating-packages.md)
- [Governance](../../../docs/ecosystem/governance.md)
- [Ecosystem overview](../../../docs/ecosystem/overview.md)
- Skill [li-ecosystem-discipline](../li-ecosystem-discipline/SKILL.md) for cross-repo work

## Do not

- Invent a different `li.toml` schema
- Skip `CHANGELOG.md` / `PUBLISH.md` for official packages
- Use `cap-jmk-real` — canonical org is **`li-langverse`**
