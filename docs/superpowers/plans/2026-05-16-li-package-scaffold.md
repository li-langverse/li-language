# Li package scaffold — setup tool, skill, and docs

> **Part of the ecosystem track.** Canonical **`li.toml` schema** and long-term UX live in [2026-05-16-li-package-manager-lip.md](2026-05-16-li-package-manager-lip.md) (§ A3). This plan covers **day-one scaffolding** before `lip init` ships in phase **8b**.

**Master plan ID:** **Pkg** (prerequisite for **8a** layout consistency and **8b** `lip init`).

---

## How this fits `lip`

| Concern | This plan (Pkg) | [lip plan](2026-05-16-li-package-manager-lip.md) |
|---------|-----------------|--------------------------------------------------|
| Create directories | `scripts/li-new-package` | `lip init` wraps same script (8b) |
| `li.toml` fields | Templates **must match** lip § A3 | **Authoritative schema** |
| `li.lock` | Not created by scaffold (optional empty note in README) | `lip install` / `lip add` (8b) |
| Dependencies | Path placeholders in comments only | Resolve git/registry (8b–8d) |
| Publish | `PUBLISH.md` stub | `lip publish` + registry (8d) |
| Tests / coverage | Package `li-tests/` smoke | `lit` 80% gate (8e) |
| Security | None (local files only) | ed25519 + `proof_digest` (8c) |

```text
  User / agent
       │
       ▼
  li-new-package  ──►  li.toml (§ A3) + src/ + li-tests/
       │
       │  (later)
       ▼
  lip init / add / install / publish
```

---

## User experience

```bash
./scripts/li-new-package my-math --kind library
./scripts/li-new-package my-tool --kind binary
./scripts/li-new-package my-lib --workspace packages
```

`lip init` (phase 8b) will be documented as:

```bash
lip init my-math          # equivalent to li-new-package + lockfile stub
```

---

## Standard directory layout

Same tree as lip plan and [li-httpd](2026-05-16-li-httpd-plan.md) `packages/` convention:

```text
packages/my-math/
  li.toml              # generated from lip § A3 template
  li.lock              # optional; lip creates in 8b
  README.md
  PUBLISH.md
  src/lib.li           # or main.li for binary
  li-tests/manifest.toml
  li-tests/smoke/builds.li
```

Workspace root `packages/li.toml`:

```toml
[workspace]
members = ["my-math", "my-net"]
```

---

## Setup tool: `scripts/li-new-package`

| Flag | Purpose |
|------|---------|
| `--kind library\|binary` | Entry layout |
| `--workspace PATH` | Append to `[workspace].members` |
| `--out DIR` | Standalone repo (default `packages/<name>`) |
| `--dry-run` | Print tree only |
| `--force` | Overwrite existing |

Templates: `scripts/templates/package/` — **generated from lip § A3** (single template source file `li.toml.template` checked into repo).

**CI:** `li-tests/tooling/li_new_package_smoke.sh`

---

## Agent skill

**Path:** `.cursor/skills/create-li-package/SKILL.md`

1. Run `./scripts/li-new-package` (never hand-roll dirs).
2. Use **lip § A3** fields only in `li.toml`.
3. Add contracts on all `def`s; register `li-tests/`.
4. Link [creating-packages.md](../../guide/creating-packages.md) and [ecosystem/overview.md](../../ecosystem/overview.md).

---

## Documentation (with lip + governance)

| Doc | Owner |
|-----|-------|
| [docs/guide/creating-packages.md](../../guide/creating-packages.md) | Pkg + lip user path |
| [docs/guide/package-layout-reference.md](../../guide/package-layout-reference.md) | Field reference → lip § A3 |
| [docs/ecosystem/overview.md](../../ecosystem/overview.md) | Pkg vs lip vs lit diagram |
| [docs/ecosystem/governance.md](../../ecosystem/governance.md) | Org policy, standards, traceability (summary) |
| [docs/ecosystem/official-packages.md](../../ecosystem/official-packages.md) | `PKG-*` registry + GitHub org repos |
| [docs/ecosystem/lip.md](../../ecosystem/lip.md) | lip plan (8b–8d) |
| [2026-05-16-li-ecosystem-governance.md](2026-05-16-li-ecosystem-governance.md) | Full governance + intl standards |
| mkdocs Guide + Ecosystem nav | All above |

**Per-package repo files** (from [governance plan](2026-05-16-li-ecosystem-governance.md)): `README`, `CHANGELOG` (Keep a Changelog), `LICENSE` (SPDX), `SECURITY.md`, `PUBLISH.md` (`PKG-` id), `docs/traceability.md`.

**CLI:** `--official` → assign `PKG-` stub, print `gh repo create li-langverse/<name>` checklist, emit traceability template.

---

## Exit gate (Pkg)

- [x] `li-new-package` emits li.toml valid against lip § A3 (`scripts/li-new-package`)
- [x] `create-li-package` skill committed (incl. `--official` + org checklist)
- [x] Guide + ecosystem overview + governance summary in mkdocs
- [x] `scripts/templates/github-repo/` for org repos
- [x] `docs/ecosystem/official-packages.md` stub with `PKG-li-language`
- [x] Smoke test in `scripts/ci.sh`
- [x] lip + governance + master plan cross-linked

**Not in Pkg exit gate:** `import`, `li.lock`, registry (8a–8d).

---

## Tasks

1. Add `scripts/templates/package/` from lip § A3
2. Add `scripts/li-new-package`
3. `li-tests/tooling/li_new_package_smoke.sh`
4. `.cursor/skills/create-li-package/SKILL.md`
5. `docs/guide/creating-packages.md`, `docs/ecosystem/overview.md` (stubs OK until execute)
6. Wire master plan tracker **Pkg** checkbox
