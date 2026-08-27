# Active agent claims (lic / httpd P0)

**Purpose:** Tell humans and other agents what is in flight so work does not collide.  
**Canonical policy:** [agent-coordination.md](agent-coordination.md) (roadmap repo).  
**Local claims file:** `coding-projects/.li-agent-coord.json` (gitignored — update when you start or finish a task).

**Last updated:** 2026-05-16 (Cursor agent — httpd P0 async codegen)

---

## In progress (do not duplicate)

| Claim ID | Owner | Repo / path | Branch | Scope | Do not touch |
|----------|-------|-------------|--------|-------|--------------|
| `httpd-p0-bench` | lic agent | `benchmarks/harness/bench_ecosystem.py`, `tier3_ecosystem/` | `dev` | Tier-3 CSV for dashboard ingest | `benchmarks/results/*.csv` format |
| `httpd-p0-async-codegen` | lic agent | `compiler/mir`, `compiler/codegen`, `runtime/li_rt.*` | `dev` | MIR `AsyncAwait`, `li_async_*` reactor stubs | Same paths without coordinating |
| `httpd-p0-net` | lic agent | `packages/li-net/` | `dev` | Trusted TCP `extern` surface (stable) | `packages/li-net` API changes |
| `httpd-p0-org` | lic agent | `scripts/push-official-package-repo.sh` | `dev` | **Done** — org mirrors pushed (see table below) | `gh repo delete`, uncoordinated force-push |
| `httpd-p0-docs` | lic agent | `docs/ecosystem/httpd-prerequisites.md`, this file | `dev` | Status tables for P0 gates | Rewriting httpd plan |

**Not claimed here (other repos):**

| Repo | Typical owner | Notes |
|------|---------------|-------|
| `lis` | harness / nginx oracle | Tier5 HTTP tests, routing TOML — do not change exploit mitigations without CVE row |
| `lip` | package manager | Resolver, publish — no compiler changes |
| `lit` | coverage CLI | Runner only |
| `roadmap` | human merge | Governance + `official-packages.md` canonical table |

---

## Official package repos (mirror from `lic`)

Monorepo path stays source until **lip** publish; org repo is the **public home** per [governance](../superpowers/plans/2026-05-16-li-ecosystem-governance.md).

| Package | Org repo | PKG id | Monorepo | Push script |
|---------|----------|--------|----------|-------------|
| li-net | `li-langverse/li-net` | `PKG-li-net` | `packages/li-net/` | `./scripts/push-official-package-repo.sh li-net --create` |
| li-std-core | `li-langverse/li-std-core` | `PKG-li-std-core` | `packages/li-std-core/` | same pattern |
| li-std-math | `li-langverse/li-std-math` | `PKG-li-std-math` | `packages/li-std-math/` | same pattern |
| li-demo | `li-langverse/li-demo` | `PKG-li-demo` | `packages/li-demo/` | template only |

**Human:** Add rows to [roadmap `official-packages.md`](https://github.com/li-langverse/roadmap/blob/main/docs/ecosystem/official-packages.md) via PR (agents do not merge governance there).

---

## Next compiler work (queued, unclaimed)

1. **epoll/kqueue** behind `li_async_poll` (real suspend/resume).
2. **Bytes** `Reader`/`Writer` + syscall codegen for `li-net`.
3. HTTP/1 parser slice in `lic` (blocked on 2f for full proof).

To **claim** a row: add an entry under `repos.lic.claims` in `../.li-agent-coord.json` and bump `updated_at`.

---

## Quick commands

```bash
# Register claim (example)
# edit ../.li-agent-coord.json — see scripts/templates/.li-agent-coord.json.example

# Push package mirror to org
set -a && source ../.env.github && set +a
./scripts/push-official-package-repo.sh li-net --create
```
