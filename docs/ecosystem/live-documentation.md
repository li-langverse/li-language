# Live documentation (lic)

<!-- DOC-ecosystem-live-docs -->

**Published handbook (MkDocs Material):** https://li-langverse.github.io/li-language/

Source: `mkdocs.yml` + `docs/` in this tree. The org mirror **`li-langverse/lic`** should publish the same site when Pages is enabled on that repo (copy `mkdocs.yml` workflow from **li-language** or add `.github/workflows/pages.yml`).

## In-repo entry points

| Topic | Path |
|-------|------|
| Master plan (PH tracker) | [2026-05-14-li-master-plan.md](../superpowers/plans/2026-05-14-li-master-plan.md) |
| Provability gaps (**G-***) | [provability-gaps.md](../verification/provability-gaps.md) |
| Phase plans | [superpowers/plans/](../superpowers/plans/) |
| Doc style / honesty | [contributing/documentation.md](../contributing/documentation.md) |
| Ecosystem vision (canonical) | [roadmap: vision-and-roadmap](https://github.com/li-langverse/roadmap/blob/main/docs/ecosystem/vision-and-roadmap.md) |
| Benchmark dashboard | [benchmarks handbook](https://github.com/li-langverse/benchmarks/blob/main/docs/handbook/README.md) |

## Cross-link discipline

1. Closing a **G-*** row → update [provability-gaps.md](../verification/provability-gaps.md) in the **same PR** as code.
2. Closing a **PH** checkbox → update [master plan](../superpowers/plans/2026-05-14-li-master-plan.md) or the linked phase plan.
3. Do not claim **`lic build` = Lean certificate** until Phase **2f** — see gap summary table.

## Enable GitHub Pages (human)

1. Repo **Settings → Pages → GitHub Actions**
2. Add workflow: build MkDocs (`pip install -r docs/requirements.txt && mkdocs build`) → `upload-pages-artifact` → `deploy-pages`
3. Set `site_url` in `mkdocs.yml` to the repo’s `*.github.io` URL

Until Pages is live, use local preview: `mkdocs serve` from repo root.
