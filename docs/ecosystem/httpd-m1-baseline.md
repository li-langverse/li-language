# li-httpd M1 baseline (2026-05-22)

**Purpose:** Clean record after landing proxy/runtime fixes on `main`, before autonomous plan loop continues.

**Plan:** [2026-05-16-li-httpd-plan.md](../superpowers/plans/2026-05-16-li-httpd-plan.md) · **Master plan:** Phase H in [2026-05-14-li-master-plan.md](../superpowers/plans/2026-05-14-li-master-plan.md)

## On `main` today (do not re-merge)

| PR | What |
|----|------|
| #153–#158 | Proxy/runtime M1, li-log, E0360 ABI |
| #162 | Autonomous plan loop seed + routing compile fix |
| #172 | Goal-directed `httpd-plan-loop.py` (`code_implementer`, stream fixes) |
| #174 | Ecosystem phase 0: LLVM 22, Lean CI, `li-tests` manifest honesty, phase **8p** plan |

**Runtime:** `runtime/li_rt_net.c`, `runtime/li_rt_httpd.c`, `runtime/li_rt_log.c`  
**Packages:** `packages/li-net-httpd`, `packages/li-http`, `packages/li-log`  
**Scripts:** `httpd_config.py`, overlap/validate/flatten, `scripts/httpd-plan-loop.py`

## Closed / superseded (2026-05-22 hygiene)

| PR | Reason |
|----|--------|
| #87, #84, #119, #130, #160, #149 | Absorbed by #173 or already on `main` via #153–#162 |
| Old httpd drafts | C/proxy experiments superseded by merged runtime work |

**Branches left on GitHub for archaeology** — do not reopen PRs; cherry-pick onto a fresh branch from `main` if needed.

## Active integration (one PR)

| Branch / PR | Content |
|-------------|---------|
| **`cursor/httpd-plan-loop-54aa` (#173)** | M1 routing, Bearer auth, TOML desugar, rate-limit/LB smokes — **rebased on `main` after #172+#174** |

Ecosystem gap loop: **`cursor/lic-ecosystem-plan-loop-54aa`** — merged (#174); continue phase 1 on a **new branch from `main`**.

## Next todos (plan loop order)

1. **m1-routing-tests** — `match_routes.li` compile + oracle green; overlap reject CI  
2. **m1-bearer-auth** — `test-auth-bearer.sh` on real `build/li-httpd`  
3. **m1-toml-desugar** — desugar golden + explain-config  
4. **m1-core** — remaining M1 parser/LB/rate-limit Li surface  
5. **w1-async-reactor** — blocked on language async (post-M1)

## Autonomous loop

```bash
export CURSOR_API_KEY=cursor_...
export LI_CURSOR_AGENTS_ROOT=/path/to/li-cursor-agents
cd lic
./scripts/httpd-plan-gates.sh
./scripts/httpd-plan-loop.py --once
```

Preflight (optional): `cd ../benchmarks && ./scripts/agent-preflight.sh`

## Agent continuation

1. Read this file + plan YAML todos.  
2. Run `./scripts/httpd-plan-gates.sh`.  
3. `./scripts/httpd-plan-loop.py --once` (or implement todo manually).  
4. Push to **#173** only; human merges after CI green.  
5. **Do not** stack new httpd PRs until #173 lands or is split.
