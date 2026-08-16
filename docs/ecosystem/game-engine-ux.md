# Game engine UX — Li v1 (proof-first)

**Status:** v1 documentation + experimental smoke only — not std promotion.  
**Research goal:** `game_engine_ux` (li-cursor-agents `config/goal-scaffolds/game_engine_ux.md`).  
**Related:** `docs/physics/GAME_DEV.md` (when present on branch), `li-tests/physics/game_runtime_smoke.li`, `packages/li-std-physics-*`.

## North star

Easy Li APIs for interactive loops and AI agent hooks; no `unsafe`; contracts on every public `def`.

## Package placement (v1)

| Layer | v1 decision |
|-------|-------------|
| Physics / simulation | `packages/li-std-physics-*` + `li-std-physics-runtime` (see GAME_DEV.md) |
| Window / input / render | **Not in std v1** — spike stays under `li-tests/` or experimental `packages/` only after `PKG-*` approval |
| Agent-facing hooks | Document verbs in ecosystem docs; implement as composable `lib.li` when `import` lands |

Do **not** create a new official org repo without roadmap `PKG-*` approval.

## Learned from (reference engines)

| System | Takeaway for Li |
|--------|-----------------|
| **Godot** | Scene tree + signals; favor small composable nodes over monolithic engine loop |
| **Bevy** | ECS + explicit systems; map to proved `def` steps and disjoint parallel writes |
| **Unity DOTS** | Data-oriented batches; align with SIMD / `parallel for` only after disjoint proof |

## Proof gate (binding)

- Every new `def`: `requires` / `ensures` / `decreases` as applicable.
- No `sorry`, no bare `cast`, no new `trusted.lean` axioms in agent PRs.
- Game loop / IO drivers remain trusted boundary per [verification overview](../verification/overview.md).

## v1 deliverables

1. This doc + cross-links from GAME_DEV.md.
2. Keep `li-tests/physics/game_runtime_smoke.li` as tier-0 smoke (total `game_physics_tick`).
3. Future: thin `packages/` spike with `lit` ≥80% before `lip publish`.

## Out of scope (v1)

Full renderer, networking, asset pipeline, benchmark claims without a [benchmarks dashboard](https://li-langverse.github.io/benchmarks/) row.

## Evidence for implement PRs

- `li-tests` path or `./li-tests/run_all.sh physics` green.
- Research session / handoff id `game_engine_ux` in PR body (Agent deliverable).
