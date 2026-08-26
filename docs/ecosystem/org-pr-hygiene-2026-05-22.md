# Org PR hygiene (2026-05-22)

Single pass across **li-langverse** to merge landed work and close superseded stacks.

## Merged to `main`

| Repo | PR | Summary |
|------|-----|---------|
| **lic** | #172 | Goal-directed `httpd-plan-loop.py` |
| **lic** | #174 | Ecosystem phase 0 (LLVM 22, Lean CI, manifest, 8p plan) |
| **roadmap** | #13 | `run-local-ci-gha-quota` agent skill |
| **benchmarks** | #69 | li-cursor-agents skill_paths fix |
| **benchmarks** | #44 | Language class/decorator docs (was draft, merged) |
| **studio**, **world**, **sim***, **ui**, **render**, **mmo**, **physics.*** | #1 each | Dependabot `actions/checkout` v4→v6 |

## Active (keep open)

| Repo | PR | Owner track |
|------|-----|-------------|
| **lic** | [#173](https://github.com/li-langverse/lic/pull/173) | Httpd M1 integration only |

## Closed (superseded — branches retained)

- **lic:** #160, #149, #87–#84, #119–#130, #134–#137, #169, #170, #122, #81, #101, #140  
- **benchmarks:** #66–#67, #45–#43, #39, #34, #32, #48, #68, tier5 drafts  
- **li-cursor-agents:** #5–#6, #9  
- **li-local-ci:** #2  
- **li-language:** #6, #8  
- **roadmap:** #12  

## Repos with zero open PRs

`lip`, `lit`, `lis`, `li-httpd`, `li-demo`, `li-net`, `li-std-core`, `li-std-math`, and package stubs without activity.

## Agent rules after hygiene

1. **New work** → branch from latest **`lic` `main`**.  
2. **Httpd** → only **#173** until merged; then branch from `main`.  
3. **Ecosystem loop** → new branch (phase 0 is on `main` via #174).  
4. Do **not** reopen closed PRs; cherry-pick from old branches if needed.
