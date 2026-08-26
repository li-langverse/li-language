# Agent-kit adoption (lip / lit / lis)

<!-- DOC-ecosystem-adoption -->

1. Clone [`li-langverse/roadmap`](https://github.com/li-langverse/roadmap) sibling to this repo.
2. Copy `scripts/sync-agent-kit.sh` from **lic** (or call `../roadmap/scripts/install-agent-kit.sh <repo-id>`).
3. Add `hooks.json.<repo>-merge` if repo-specific hooks are needed.
4. CI: `./scripts/check-agent-kit-sync.sh` on PRs.
5. **PR-only** on `main` — branch protection + 1 approval.

Benchmarks: register repo paths in [`li-langverse/benchmarks`](https://github.com/li-langverse/benchmarks) `catalog.toml`.
