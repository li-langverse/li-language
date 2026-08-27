# Li Master Implementation Plan (rev. 5)

> **For agentic workers:** Execute phases in order. Python 3.14 typing parity is the typechecker north star — not a minimal v1 subset.
>
> **Human-only actions:** If a task needs something **you cannot do in code** — create a GitHub repo, org/app settings (Renovate, branch protection, secrets), OAuth/PAT, DNS, billing, or clicking UI in github.com — **stop and ask the human explicitly** before proceeding. Do not assume `gh` login, org-wide access, or that settings already exist. List exact steps; wait for confirmation.
>
> **Repos:** Before a **new** `li-langverse/*` repository, ask the human (see [§ Repository separation](#repository-separation--when-to-create-repos)). Phases **through 8d** need only **`lic`**, **`lip`**, **`lit`** (already created).

**Goal:** Open-source language: **(1) Lean provability (2) Nim syntax (3) LLVM speed** — in that priority order.

**Strict by default:** Proof, security, and performance gates are always on at maximum; there is **no optional provability** ([strict-by-default.md](../ecosystem/strict-by-default.md)). Tracker items close **implementation** gaps — they do not add implicit opt-out.

**Reliability target:** Li is **not a runtime-interpreted language**. User logic is **fully static**: types, contracts, memory, parallelism, and (for release) bounds/shapes are resolved at **`lic build`** + Lean — **not** at process start with dynamic checks. Push failures **left** until runtime error rate for proved programs approaches **zero** (only the capped **trusted** `IO` / platform surface remains outside user proofs).

**Honest status (docs):** [Provability gaps (today)](../verification/provability-gaps.md) — living register of what is **not** proved/wired yet (Lean gate, heuristic parallel policy, decorator parse-only, …). Update that file when closing a phase gate.

**Architecture:** C++ compiler → MIR → LLVM 22 (sole backend). Bootstrap to self-host later. No VM, no JIT for user code, no runtime decorator dispatch.

**GitHub org ([`li-langverse`](https://github.com/li-langverse)) — three repos (locked):**

| Repo | URL | Owns |
|------|-----|------|
| **`lic`** | https://github.com/li-langverse/lic | Compiler (`lic` CLI), `compiler/`, `li-tests/`, semantics, runtime, stdlib |
| **`lip`** | https://github.com/li-langverse/lip | Package manager, registry, `li.lock`, `lip publish` |
| **`lit`** | https://github.com/li-langverse/lit | Test runner, **≥80%** line coverage gate (CLI v1) |

**This workspace** is the compiler tree until fully migrated/pushed to **`lic`**. Replaces canonical **`li-langverse/li-language`** (archive or redirect when migration completes).

**Design spec:** `docs/superpowers/specs/2026-05-14-li-language-design.md`

**Ecosystem (agents — read before cross-repo work):** [agent-coordination.md](../ecosystem/agent-coordination.md), [engineering-standards.md](../ecosystem/engineering-standards.md), [vision-and-roadmap.md](../ecosystem/vision-and-roadmap.md).

---

## Vision, roadmap, and agent discipline

### Where to record vision

| Change | Update |
|--------|--------|
| Language pillars, compiler phases, org policy | **This master plan** (+ language design spec if normative) |
| Quarterly / public milestones | [`li-langverse/roadmap`](https://github.com/li-langverse/roadmap) + [benchmarks dashboard](https://li-langverse.github.io/benchmarks/) |
| Single package scope | That package’s `README`, `PUBLISH.md`, `docs/traceability.md` only |
| li-httpd product | [2026-05-16-li-httpd-plan.md](2026-05-16-li-httpd-plan.md) + **`lis`** |

Agents **must not** treat package README as the only vision doc when the change affects multiple repos or pillars.

### Non-negotiable engineering (all agents)

**Strict-by-default:** See [strict-by-default.md](../ecosystem/strict-by-default.md). Agents must not treat proof, CVE suites, or benches as optional; explicit downgrades only.

**Strict gates — no exceptions without human approval and a tracked PH-/issue:**

1. **Functionality** — spec/REQ correct; tests green; no silent wrong behavior.
2. **Security** — [engineering-standards § CVE](../ecosystem/engineering-standards.md#security--cve-discipline); `run_security` / exploit harness / fuzz per repo; map to catalog.
3. **Performance** — measured; benchmark regressions investigated; no “fast” without data.

Agents have freedom on structure and naming; **not** on skipping these three.

### Learn from other ecosystems (implementing Li features)

Before shipping ecosystem tooling (lip, lit, httpd, std split):

- Survey **2–4** mature systems (Cargo, nginx, npm, Envoy, …).
- Adopt **algorithms and practices** that fit Li: **easy, AI-first, provable, blazingly fast**.
- Document **“Learned from”** in the phase plan or ADR; **do not** port C/nginx verbatim.

Details: [engineering-standards.md](../ecosystem/engineering-standards.md).

---

## Repository separation — when to create repos

### What lives where (locked)

| Repo | URL | Contains | Does **not** contain |
|------|-----|----------|----------------------|
| **`lic`** | https://github.com/li-langverse/lic | `compiler/`, `runtime/`, `li-tests/`, `docs/semantics/`, `std/` (until split), `scripts/li-new-package`, `examples/`, `benchmarks/` harness tied to compiler | `lip`, `lit`, registry index, package lockfiles |
| **`lip`** | https://github.com/li-langverse/lip | `lip/` CLI sources, `registry/`, `li.lock` format, publish CI, package-manager docs | Compiler, `lit` sources, `li-tests/` conformance |
| **`lit`** | https://github.com/li-langverse/lit | `lit/` CLI sources, coverage aggregation, test-manifest runner, **80%** gate, lit docs/fixtures | Compiler, resolver, registry |

**Never create** separate repos for: registry (stays in **`lip`**), or **`lit`** duplicated inside **`lip`**.

### Sufficient today (phases 0–8d) — no new org repos

These three are **enough** until a row in [Future org repos](#future-org-repos-agent-must-remind-human) applies:

- [`lic`](https://github.com/li-langverse/lic) — compiler + **8a**, **8e-li**, **Pkg**, phases 0–7, **H** prep in-tree
- [`lip`](https://github.com/li-langverse/lip) — **8b–8d**
- [`lit`](https://github.com/li-langverse/lit) — **8e**

**8-repo exit gate:** each repo pushed, `origin` set, CI green (or documented skip). Until then, note *push pending* in the tracker.

### Future org repos — agent **must remind human**

**Rule for agents — ask the human first:** Any action that is **not** a local code/docs change in the workspace requires **explicit human approval** before the agent continues. Examples:

| Human must do | Agent asks with |
|---------------|-----------------|
| Create / rename org repo | Repo name, visibility, link to checklist |
| GitHub org or repo **settings** | Exact path in UI (Settings → …) or `gh` command **for human to run** |
| Install org app (Renovate, etc.) | App URL, scope, which repos |
| Secrets / tokens / deploy keys | Name, repo; never paste secrets in chat |
| Branch protection, rulesets | Repo, required checks |
| Watch / team notifications | Team name, which repos to watch |
| Transfer `li-language` → `lic` | Steps; confirm when done |

**New org repo workflow** (subset of above):

1. Check whether the repo already exists on GitHub.
2. If **no**, post **“Action needed from you”** with: repo name, purpose, phase ID, [governance checklist](2026-05-16-li-ecosystem-governance.md#repo-creation-checklist-human-or-agent).
3. **Wait for confirmation** before implementing or pushing.
4. Do **not** request “authorize all orgs” OAuth; human uses UI or fine-grained PAT scoped to `li-langverse`.

| Phase / trigger | Create repo | Name pattern | Why |
|-----------------|-------------|--------------|-----|
| Stdlib split | When `std/` outgrows **`lic`** or listed in `official-packages.md` | `li-std-<area>` e.g. `li-std-core`, `li-std-math` | Independent semver / second consumer; **register in `li-downstream-repos.txt`** |
| Phase **H** (httpd stack) | Before httpd needs standalone publish | `li-http`, `li-net`, `li-tls`, … per [httpd plan](2026-05-16-li-httpd-plan.md) | Infra packages, own CI |
| Official package promotion | `li-new-package --official` + second consumer or **1.0.0** | `li-<name>` | [Governance](2026-05-16-li-ecosystem-governance.md) promotion |
| Registry package (third-party) | **Never** in `li-langverse` unless adopted | author’s org | `lip publish` only |

**Stay in `lic` monorepo** (`packages/<name>/`) when: experimental, single consumer, or blocked on **8a**/**8b** — document promotion in CHANGELOG when splitting out.

**Human checklist** (after agent reminder): [governance § Repo creation checklist](2026-05-16-li-ecosystem-governance.md#repo-creation-checklist-human-or-agent); templates in `scripts/templates/github-repo/`.

### Cross-repo dependency notifications (every official package)

When **`lic`**, **`lit`**, **`lip`**, or any **`li-std-*` / `li-*`** package releases, **dependents must be notified** so maintainers bump pins and CI. Required for **every** repo under `li-langverse` (and documented for third-party `lip` consumers in **8d**).

**Dependency layers (what can change):**

| Upstream | Pinned in dependents via | Examples |
|----------|---------------------------|----------|
| **`lic`** (compiler, runtime, Lean) | `li-toolchain.toml` → `lic_version` / `lic_commit` | **lip**, **lit**, all `li-*` packages |
| **`lit`** | `li-toolchain.toml` → `lit_version` | **lip** (publish/install) |
| **`lip`** | optional; registry client version | infra only |
| **Registry packages** | `li.toml` + `li.lock` | app/libs (`linalg`, `li-std-core`, …) |

**GitHub setup (mandatory per repo — templates in Pkg / governance):**

| Mechanism | Purpose |
|-----------|---------|
| **`li-toolchain.toml`** | Explicit pins; single source of truth for CI |
| **Dependabot** (`.github/dependabot.yml`) | GitHub Actions bumps; **custom** entry for `li-toolchain.toml` where supported |
| **Renovate** (optional org bot, `renovate.json`) | Regex bumps on `lic_version`, `lit_version`, `li.lock` git SHAs |
| **`lic` release workflow** | On tag `v*`, `repository_dispatch` → registered downstream repos (list in **`lic`** `.github/li-downstream-repos.txt`) |
| **Downstream workflow** (`.github/workflows/ecosystem-upstream.yml`) | `on: repository_dispatch` + `workflow_dispatch`; opens issue or PR to bump toolchain |
| **Watch / team** | Org team `@li-langverse/maintainers` **Watching** releases on **`lic`**, **`lit`**, **`lip`** |
| **CI gate** (`scripts/check-li-toolchain.sh`) | Warn or fail if pin &lt; latest `lic` release (strict on `main` for official repos) |

**Phase **8-sync** (ecosystem — after **8-repo**, parallel with **8b**):**

| Deliverable | Repo |
|-------------|------|
| `scripts/templates/github-repo/dependabot.yml` | **`lic`** (source); copied by `li-new-package --official` |
| `scripts/templates/github-repo/ecosystem-upstream.yml` | same |
| `lic` `.github/workflows/notify-downstream.yml` + `li-downstream-repos.txt` | **`lic`** |
| `scripts/check-li-toolchain.sh` | **`lic`**; consumed by **lip** / **lit** / packages |
| Column `depends_on` in [official-packages.md](../ecosystem/official-packages.md) | **`lic`** docs |

**Agent rule:** When scaffolding or promoting an **official** package, **enable notification templates** and add the repo to **`lic`** `li-downstream-repos.txt`. Remind human if **Renovate** org app is not installed.

**Human one-time (org):** Install [Renovate](https://github.com/apps/renovate) on `li-langverse` (scoped to org) *or* rely on `repository_dispatch` + manual bumps until Renovate lands.

**Exit gate:** Bump `lic` patch tag in a test repo → downstream receives dispatch/issue; Dependabot PR for an Action pin in **lip**.

---

## Phase map (revised)

| Phase | Focus | Plan file | Exit gate |
|-------|-------|-----------|-----------|
| 0 | C++ CMake + LLVM bootstrap | `2026-05-14-phase-00-bootstrap.md` (needs C++ rewrite) | Hello binary via LLVM |
| 1 | Nim lexer/parser | `2026-05-14-phase-01-lexer-parser.md` | Parse fixtures |
| 2a | Types: scalars, unions, literals, aliases | `2026-05-14-phase-02a-types-core.md` (new) | mypy-parity fixtures pass |
| 2b | Types: generics PEP 695, Callable, Protocol | `2026-05-14-phase-02b-types-generics.md` (new) | Protocol structural tests |
| 2c | Types: TypedDict, enums, collections | `2026-05-14-phase-02c-types-collections.md` (new) | dict/list/tuple tests |
| 2d | Borrow + effects + `array[N,T]` | `2026-05-14-phase-02-typechecker.md` | borrow error tests |
| 2g | **`def`**, Nim `object`, field `private`/`public`, `import` (v1) | `.cursor/plans/nim_oop_and_def_e5f70493.plan.md` | `li-tests/encapsulation/` green |
| **2j** | **Full OOP** — methods/`self`, method visibility, traits, inheritance, object write-back | [2026-05-20-li-oop-roadmap.md](2026-05-20-li-oop-roadmap.md) | 2j-a…f exit gates in OOP plan |
| 2h | **Python-math surface** (`**`, `//`, `%`, `for`/`range`, contract sugar, `old`) | same plan (Phase A0) | `li-tests/math_syntax/` green |
| **2i** | **Math / linalg surface** (infix `*`, `+`, `dot`, `A @ B` — not user `simd(...)`) | [2026-05-16-li-math-linalg-surface.md](2026-05-16-li-math-linalg-surface.md) | `li-tests/math_linalg/`; docs samples |
| 2e | Contracts + refinements | [proof-corpus-roadmap](../verification/proof-corpus-roadmap.md) | **Partial (PR #83):** call-site `requires`, refinements, AutoVC |
| 2f | Lean 4 verify | same + [provability-gaps](../verification/provability-gaps.md) | **Partial:** open-VC gate; `contracts_verify` 16/16; full kernel **G-lean** open |
| 3 | MIR + LLVM codegen | `2026-05-14-phase-03-mir-codegen.md` | `lic build` |
| 4 | Stdlib + runtime + deferred annotations | `2026-05-14-phase-04-runtime-stdlib.md` | hello + collections run |
| 5 | Tetris | `2026-05-14-phase-05-tetris.md` | playable game |
| 5b | Benchmarks & sims | `2026-05-14-benchmarks-and-simulations.md` | **Verified** Tier 2 physics + cross-lang CSV + **X plots** |
| 6 | Self-host (post-live) | `2026-05-14-phase-06-self-host.md` | `lic` built by li (bootstrap seed) |
| 6b | Agent skill for writing Li | same § Remaining roadmap (final step) | Distill the language-authoring knowledge from the port into `.cursor/skills/write-li-language/SKILL.md` once the compiler is self-hosted |
| 7 | Native HPC (SIMD + OpenMP) | `2026-05-14-phase-07-native-hpc.md` | Pure-Li simd_dot + md; race suite; fuzz daily |
| **7d** | **Execution decorators** | `.cursor/plans/li_execution_decorators_7c6e3b42.plan.md` | **Partial:** `@vectorized` scope (#150); **G-par** disjoint= open |
| **7e** | **Math → SIMD lowering** | [2026-05-16-li-math-linalg-surface.md](2026-05-16-li-math-linalg-surface.md) § 7e | **Partial:** pure-Li tier-1 (#148); perf bar open |
| **H** | **li-httpd** (proved agent gateway) | `2026-05-16-li-httpd-plan.md` | **M1 partial:** epoll static/sendfile + proxy (#153/#156); **li-log** package stub (access sink + redact); **next:** M1 ship gate Lean + Li reactor |
| **Obs** | Fuzz dashboard + plan canvas | `scripts/export-fuzz-status.sh` + `canvases/*.canvas.tsx` | Nightly updates `.canvas.data.json` sidecars |
| **Pkg** | **Package scaffold** (dirs + `li.toml`) | [2026-05-16-li-package-scaffold.md](2026-05-16-li-package-scaffold.md) | `li-new-package` + skill + creating-packages guide (**`lic`** repo) |
| **8-repo** | **Org repos live + push/CI** | [2026-05-16-li-package-manager-lip.md](2026-05-16-li-package-manager-lip.md) | **`lic`**, **`lip`**, **`lit`** on GitHub; toolchain pins; bootstrap smoke |
| **8a** | **Modules + workspace** `lic build` | same § 8a | **`lic`**: `li-tests/modules/`; same `li.toml` as Pkg |
| **8e-li** | **`lic --coverage-instrument`** | same § 8e | **`lic`**: LLVM profiling flag for coverage |
| **8e** | **`lit` + coverage gate** | same § 8e | **`lit`**: ≥80% on publish; **`std/**` in `lic`**: **100%** (`check-stdlib-coverage.sh`) |
| **8b** | **`lip` path/git + lockfile** | same § 8b | **`lip`**: `lip init` → scaffold; `lip install` reproducible |
| **8c** | **Signatures + proof digests** | same § 8c | **`lip`**: ed25519; `proof_digest` in `li.lock` |
| **8d** | **Registry + `lip publish`** | same § 8d | **`lip`**: hybrid git + registry; CI runs **`lit`** + `lic build` |
| **8-sync** | **Upstream dependency notifications** | [governance § Cross-repo notifications](2026-05-16-li-ecosystem-governance.md#cross-repo-dependency-notifications) | Dependabot + `lic` release dispatch; every official repo |
| **8p** | **Parallel compile + CI throughput** | [§ 8p below](#phase-8p--parallel-compile--ci-throughput) | `li-tests` / workspace / `lic build` use host cores; local-ci ≲½ wall time on 8+ cores |
| **Doc** | **Documentation + provability honesty** | [§ Doc below](#documentation--provability-honesty-cross-cutting) | Gap register current; handbook matches `lic`; no overclaim |

**Old 2-week schedule is void.** Type parity alone is ~6 months part-time.

**Doc runs in parallel** with compiler phases — not after “v1”. Every compiler PR that changes the proof surface must touch the gap register (see below).

---

## Ecosystem — packages (`lic`, `lip`, `lit`, scaffold)

Three **org repos** + scaffold in **`lic`**. **One `li.toml` schema** (authoritative in [lip plan § A3](2026-05-16-li-package-manager-lip.md)):

| Plan | Repo | Role | Ship when |
|------|------|------|-----------|
| [2026-05-16-li-package-scaffold.md](2026-05-16-li-package-scaffold.md) | **`lic`** | `scripts/li-new-package`, agent skill, user guide | **Pkg** — before `import` |
| [2026-05-16-li-package-manager-lip.md](2026-05-16-li-package-manager-lip.md) | **`lip`** + **`lit`** | lockfile, registry, proof+sig+coverage | **8-repo → 8a → 8e-li → 8e → 8b → 8c → 8d** |

```text
8-repo (lic + lip + lit on GitHub, push + CI)
  Pkg (scaffold in lic) ──► 8a (import/workspace in lic)
                                    ├──► 8e-li (coverage flag in lic)
                                    ├──► 8e (lit repo: lit CLI + 80% gate)
                                    └──► 8b (lip repo) ──► 8c ──► 8d
                              8-sync (notify on lic/lit/std releases) ──► all official repos
```

**`lip publish` / registry install:** `lic build` + **`lit test --coverage` ≥ 80%** + ed25519 (unsigned third-party rejected by default).

**Policy:** Do not advertise third-party `lip install` until **8a + 8c + 8e**. Scaffold and path-only workspaces are fine earlier.

**Docs:** `docs/ecosystem/*` in **`lic`**; `docs/lip.md` + `registry.md` in **`lip`**; `docs/lit.md` in **`lit`**; threat model `docs/verification/packages.md` in **`lic`**.

**Governance:** [2026-05-16-li-ecosystem-governance.md](2026-05-16-li-ecosystem-governance.md) — org **`li-langverse`**, compiler home **`lic`** (not `li-language`), **SemVer/SPDX/Changelog**, **PKG/REQ** traceability.

**Also uses packages:** [li-httpd plan](2026-05-16-li-httpd-plan.md) — `li-new-package` in **`lic`**; resolve with **`lip`** after **8b**; promote to **`li-langverse/li-*`** per governance.

---

## Compile-time reliability (no user runtime)

Li maximizes **reliability** by eliminating **user-visible runtime failure modes** wherever proofs allow.

| Class of failure | Li approach | When it fails |
|------------------|-------------|---------------|
| Type / shape / dim mismatch | Static checker + refinements | **`lic build`** |
| Out-of-bounds index | Refinement or proof; no silent release-only checks | **`lic build`** |
| Data races / bad `parallel` | Proved disjointness + borrow | **`lic build`** |
| Non-termination | `decreases` / totality (except declared `IO`) | **`lic build`** |
| Contract violation | VC → Lean | **`lic build`** |
| Decorators (`@parallel`, `@cpu`, …) | **Compile-time** elaboration to MIR; no runtime wrapper | **`lic build`** if illegal |
| Math (`A @ B`, `dot`) | **Compile-time** lowering to SIMD/OpenMP | **`lic build`** on shape errors |
| User `decorator def` | Macro expand → whitelist; no runtime hook | **`lic build`** |

**Not in user code:** `Any`, `unsafe`, `sorry`, runtime reflection on types, dynamic decorator registries, “try at runtime” bounds.

**Minimal native layer (not “the language runtime”):** `li_rt.c` + `trusted.lean` — bounded `IO`, debug hooks, OpenMP thread team. This is **platform glue**, not a place to hide user semantics. Release builds after successful **`lic build`** must not rely on catching user logic bugs via panic.

**Debug-only (optional CI):** TSan/fuzz find **compiler** bugs — not a user safety net.

**Implication for 7d / 7e / 2i:** decorators and math are **static sugar** only; performance and correctness are fixed at compile time.

The table above describes the **target**. Where **`lic` has not established mathematical provability yet**, see [Provability gaps](../verification/provability-gaps.md) and [compiler task → gap map](#compiler-tasks-vs-proof-gaps) below.

---

## Phase 8p — Parallel compile & CI throughput

**Problem:** Full `li-tests` + `scripts/local-ci.sh` often take **5–15+ minutes** on a devbox because almost every gate runs **`lic build` sequentially**, even when the machine has many idle cores. Only the **C++ compiler** build (`./scripts/build.sh` → Ninja `-j`) uses parallelism today.

**Goal:** Use **all host cores** for orchestration and compilation where proofs and determinism allow — so agents and humans do not wait “ages” on every green gate.

**Not the same as runtime `parallel for`:** Phase **8p** speeds up **building and verifying** Li programs. User-code multi-core execution stays in phases **7** / **7d** / **7e** (`--threads=N`, OpenMP).

### Layers (what can run in parallel)

| Layer | Today | 8p target |
|-------|--------|-----------|
| **C++ `lic` binary** (CMake/Ninja) | **Done** — `LI_BUILD_JOBS` / `nproc` in `scripts/build.sh` | Keep; document in getting-started |
| **`li-tests/run_all.sh`** | One `lic build` at a time (~200 invocations) | **`LI_TEST_JOBS`** (default: host cores); pool over manifest rows |
| **`lic-workspace-build.sh`** | Sequential workspace members | Parallel member builds when isolated |
| **`benchmarks/harness/bench.py` tier 0** | Runs full `run_all` then `verify.py` | Reuse parallel runner; avoid duplicate full sweeps where possible |
| **Single `lic build file.li`** | Single-threaded frontend pipeline | **`lic build --jobs=N`** / `LI_COMPILE_JOBS` — wire reserved flag in `compiler/lic/main.cpp` |
| **Lean / AutoVC** | Shared `build/generated/AutoVC.lean` races parallel jobs | Per-job `LI_BUILD_DIR` or VC-hash cache; `lake -j` for package builds |

### Sub-phases & exit gates

| Sub | Task | Exit gate |
|-----|------|-----------|
| **8p-a** | **Parallel test orchestration** — `run_all.sh` accepts `-j N` / `LI_TEST_JOBS`; each worker gets isolated `build/` subtree (or temp dir) so AutoVC/Lean do not stomp | **196/196** manifest green with `-j8` on Linux; `LI_TEST_JOBS=1` reproduces current sequential logs |
| **8p-b** | **Parallel workspace + package smoke** — `lic-workspace-build.sh`, `lit` manifest runner (when split) use same job pool | Workspace build wall time ∝ `members / cores` (within ~1.3× ideal) |
| **8p-c** | **Compiler frontend jobs** — implement `--jobs` / `LI_COMPILE_JOBS` for independent MIR/LLVM units inside one TU where safe; document vs LLVM’s own threading | Measurable speedup on large `packages/li-net-httpd/src/lib.li` smoke |
| **8p-d** | **CI policy** — `scripts/ci.sh` exports sensible defaults; GHA `ubuntu-latest` sets `LI_TEST_JOBS`; skill `run-local-ci-gha-quota` mentions `-j`; optional wall-time budget in baseline | `local-ci.sh` logged “wall_s” in ecosystem baseline; target **≤50%** vs sequential on 8-core devbox |

### Safety & determinism rules

1. **Default parallelism = host cores**; **`LI_TEST_JOBS=1`** / **`LI_BUILD_JOBS=1`** for bisect and golden logs.
2. **No shared `build/generated/AutoVC.lean`** across concurrent workers without isolation — race caused flaky Lean failures in parallel experiments.
3. **Proof surface unchanged** — parallel builds must not skip Lean or open-VC policy; only schedule work faster.
4. **GHA quota:** parallel local CI is the primary win when Actions minutes are exhausted; same gates, shorter wall clock.

### Agent priority

Implement **8p-a** before **8p-c** (largest CI win for least compiler risk). Track in [provability-gaps](../verification/provability-gaps.md) only if parallel scheduling changes what “green” means.

**Learned from:** Cargo `build -j`, Ninja, Bazel remote/local pools — adopt **job pool + hermetic per-task `target/`**, not distributed compilation in v1.

---

## Deferred — GHCR release images (prebuilt `lic`)

**Today:** `ghcr.io/li-langverse/lic-ci:*` images are **development/CI toolchain only** (OS + LLVM 22 + cmake/ninja/python). They do **not** ship a built `lic` binary; `./scripts/local-ci.sh --docker` mounts the repo and runs `scripts/build.sh` + `ci.sh` so the compiler always matches the branch under test.

**Later (after compiler stabilizes):** publish **release** container tags that include a **pinned `lic` release** (and optionally `lit`/`lip` smoke tools) for consumers who should not compile from source. **Not started** while `lic` still changes frequently (httpd, contracts, LLVM pins, manifest gates).

| When | Deliverable |
|------|-------------|
| **Now** | `debian12-llvm22` / `ubuntu24-llvm22` toolchain images; podman preferred locally |
| **After stable `lic` cadence** | e.g. `lic-vX.Y.Z` image with baked binary + documented `LI_LIC_VERSION` |
| **Gate to start** | Few breaking changes per month; release tags drive downstream pins; CI green without rebuilding compiler on every small change |

See [local-ci-docker-images.md](../ecosystem/local-ci-docker-images.md).

---

## Documentation & provability honesty (cross-cutting)

**Problem:** Handbook and specs must not read as if Lean, full disjoint proofs, decorators, or math lowering already ship when they do not. Contributors need a **single gap register** and **compiler-task linkage**.

**Canonical register:** [docs/verification/provability-gaps.md](../verification/provability-gaps.md) (`G-*` IDs).

### Phase Doc — sub-phases

| Sub | Task | Exit gate |
|-----|------|-----------|
| **Doc-a** | **Gap register** — keep `provability-gaps.md` current; link from index, overview, master plan, mkdocs | Register lists every open **G-*** row; last-updated date |
| **Doc-b** | **Handbook audit** — every `docs/language/*`, `docs/guide/*`, `docs/compiler/*` uses *target vs today* where spec ≠ `lic` | No page implies Lean runs on `lic build` without a status note |
| **Doc-c** | **Spec stubs** — decorator, math, language design cross-link gaps | `2026-05-16-li-execution-decorators.md`, `2026-05-16-li-math-linalg-surface.md` point at **G-*** |
| **Doc-d** | **Contributing rule** — [documentation style](../contributing/documentation.md) § Provability | PR template / agent skill: “updated gap register?” |
| **Doc-e** | **CI doc check** (optional) — `scripts/check-doc-provability-claims.sh` greps for forbidden phrases without nearby “gap” link | Fails on `proofs closed` without `provability-gaps` in changed docs |

**Doc is “done” for a release tag only when:** open **G-*** rows match reality and handbook does not overclaim.

### PR rule (binding for compiler work)

When a PR implements or partially implements a proof-related compiler feature:

1. Update **[provability-gaps.md](../verification/provability-gaps.md)** — move **G-*** row to **Partial** or **Done** with one-line evidence (file, test suite, CLI flag).  
2. Update affected handbook pages listed in the gap doc § *Documentation that must stay aligned*.  
3. If the feature is still **Partial**, add an admonition on the handbook page (mkdocs `!!! note "Provability status"`).  
4. Mention **G-*** ID in the PR description.

### Compiler tasks vs proof gaps

Maps **master plan phases** to gap IDs and what “mathematical provability established” means for **`lic`** (not aspirational spec text).

| Phase | Compiler / `lic` work | Gap ID(s) | Proof established when… | Doc + tests to update |
|-------|----------------------|-----------|-------------------------|------------------------|
| 2e | VC generation | **G-vc** | VCs emitted per `requires`/`ensures`/loop clauses | gaps, contracts-and-proofs, build-pipeline |
| 2f | Lean 4 in `lic build` | **G-lean**, **G-vc** | `lic build` invokes Lean; open goals → exit 1 | gaps, why-provable, overview, semantics README |
| 2f | `Core.lean` | **G-trust** | Typing + contract rules in Lean, not stub | semantics/README |
| 3 | MIR bounds / refinement | **G-bnd** | Release path does not rely on `li_bounds_fail` for proved indices | architecture, numerics |
| 7b | Structured `disjoint=` | **G-par** | Disjointness from AST, not `policy.cpp` strings | simd-parallel, gaps |
| 7d-a | Decorator parse | **G-dec** | (partial) parse only — mark **Partial** | decorators spec |
| 7d-b–e | Decorator elaborate + exploits | **G-dec** | Elaboration + `decorator_exploits` CI green | language/decorators.md (new), fast-math guide |
| 2i | Math surface types | **G-math** | `A @ B` / `dot` shape errors at typecheck; **P-linalg** closed VCs (#151) | linear-algebra.md, math spec, `contracts_verify/linalg_*` |
| 7e | Math → SIMD MIR | **G-math** | Tier 1 Li sources math-only; lowering proved or documented | gaps, benchmarks plan, `discharge_linalg_int_lean.sh` |
| 2f | P-linalg corpus | **G-lean**, **G-math** | Closed dot/sum/matmul-entry; loop dot open | [proof-corpus-roadmap](../verification/proof-corpus-roadmap.md) |
| 2g / 2h | `def`, math syntax | — | Syntax only unless tied to VC | language overview |
| 4 | Deferred annotations | **G-ann** | PEP 649 resolve at check time | architecture diagram |
| H / GPU | Device / async | **G-gpu**, **G-async** | Device laws in Lean + codegen | decorator spec, effects |

**Not proof (document as limits):** **G-wrong-spec**, **G-hw**, **G-meta** — stay in [why-provable](../compiler/why-provable.md).

### Documentation gaps to close (checklist)

Track in phase **Doc** until each is checked:

- [x] **Doc-a** — gap register complete and linked site-wide (mkdocs nav; README audit ongoing)  
- [x] **Doc-b** — [linear-algebra.md](../language/linear-algebra.md) stub + [decorators.md](../language/decorators.md); fast-math guide audit partial  
- [x] **Doc-b** — [language design spec](../specs/2026-05-14-li-language-design.md) banner: “implementation status → provability-gaps”  
- [x] **Doc-c** — phase plan files (02, 03, 07) link **G-*** in exit gates  
- [x] **Doc-d** — `.cursor/skills/build-li-master-plan/SKILL.md` includes gap-register update step  
- [x] **Doc-e** — `check-doc-provability-claims.sh` in `scripts/ci.sh`  

---

## Stack decision record

| Decision | Choice | Rejected |
|----------|--------|----------|
| Execution model | **AOT compile** + proved native code | VM, JIT user code, runtime eval |
| Compiler host | **C++** | Zig (rejected), Rust v0 (slow link) |
| Codegen | **LLVM 22 only** | Cranelift, interpreted fallback |
| Type baseline | **Python 3.14** (static only) | `Any`, gradual typing, runtime `isinstance` |
| Syntax | **Nim-like core** + **mathematical surface** (`def`, `for`, infix `*`, matrix `@`, reductions) | User-facing `simd(...)` / `__li_simd_*`; Java `class` |
| License | **MIT OR Apache-2.0** | Proprietary |

---

## Sub-plan index

| File | Status |
|------|--------|
| `2026-05-14-phase-00-bootstrap.md` | **Done** — C++ + CMake + LLVM smoke |
| `2026-05-14-phase-01-lexer-parser.md` | Valid structure; retarget paths to `compiler/` |
| `2026-05-14-phase-02-typechecker.md` | Partial; split into 2a–2d |
| `2026-05-14-phase-03-mir-codegen.md` | LLVM-only codegen |
| `2026-05-14-phase-04-runtime-stdlib.md` | Add PEP 649 deferred annotations |
| `2026-05-14-phase-05-tetris.md` | Valid |
| `2026-05-14-benchmarks-and-simulations.md` | Physics, ML, cross-lang harness |
| `2026-05-14-plots-and-social.md` | **X-ready** benchmark + test plots |
| `2026-05-16-li-httpd-plan.md` | Proved AI/agent HTTP gateway (li-httpd); nginx oracle only |
| `.cursor/plans/nim_oop_and_def_e5f70493.plan.md` | Phase **2g** (objects/visibility/`def`) + **2h** (Python-math syntax audit) |
| [2026-05-20-li-oop-roadmap.md](2026-05-20-li-oop-roadmap.md) | Phase **2j** (full OOP — methods, traits, inheritance; **not** httpd) |
| `2026-05-16-li-package-scaffold.md` | **Pkg** — scaffold tool, skill, guide (same `li.toml` as lip) |
| `2026-05-16-li-package-manager-lip.md` | **8-repo, 8a–8d, 8e, 8e-li** — three repos (`lic`/`lip`/`lit`), registry, proof+sig+coverage |
| `2026-05-16-li-ecosystem-governance.md` | GitHub org policy, intl doc standards, PKG/REQ traceability, org repo templates |
| `.cursor/plans/li_execution_decorators_7c6e3b42.plan.md` | Phase **7d** — `@` decorators on `def`/`for`/`while`; reserved stdlib names; `decorator_exploits` suite |
| [2026-05-16-li-math-linalg-surface.md](2026-05-16-li-math-linalg-surface.md) | Phase **2i** + **7e** — math notation in source; compiler lowers to SIMD/OpenMP; Tier 1 cross-lang benches |
| [provability-gaps.md](../verification/provability-gaps.md) | **Doc-a** — living **G-*** register; update on every proof-surface PR |
| [2026-05-22-parallel-compile-ci.md](2026-05-22-parallel-compile-ci.md) | Phase **8p** — parallel `li-tests`, workspace builds, `--jobs` frontend |

**2g / 2h / 2i:** After **2d**, run **2g** + **2h** in parallel; then **2i** (linalg surface). User-facing functions are **`def` only**; numerics read like **math** (`C += A @ B`, `y[i] = alpha * x[i] + y[i]`), not `simd(...)`. Finish **2g–2i** before widening **2e** method VCs. `simd[T,N]` / `__li_simd_*` only in compiler appendix.

**7 / 7d / 7e:** Complete **7a–7c** (SIMD MIR, `parallel for`, benchmarks) first. **7d** = decorators (`@parallel`, `@vectorized`, `@cpu`, …). **7e** = lower **math expressions** to that MIR (users never write `simd(...)` in docs/benches). Depends on **2i** + **7b**. Cross-lang perf: [benchmarks plan](2026-05-14-benchmarks-and-simulations.md) Tier 1 (`simd_dot`, `matmul_*`), policy **≤1.2× C++**. Specs: `docs/superpowers/specs/2026-05-16-li-execution-decorators.md`, `docs/superpowers/specs/2026-05-16-li-math-linalg-surface.md`.

---

## Phase completion tracker

- [x] Phase 0 — C++ / LLVM bootstrap
- [x] Phase 1 — Lexer + Parser
- [x] Phase 2a — Type core (typecheck + prove_reject + race policy gates; full mypy parity pending)
- [x] Phase 2b — Generics + Protocol (PEP 695 params, TypeApp, Callable, Sized protocol)
- [x] Phase 2c — Collections + TypedDict (list/dict/tuple, named tuple, enum)
- [x] Phase 2d — Borrow + effects (lexical borrowck, raises IO/Alloc)
- [x] Phase 2g — `def`, `object` + field `private`/`public`, minimal `import` (`encapsulation` suite green; import parse-only; **not** full OOP)
- [x] Phase 2j — Full OOP surface — **2j-a…f done** (#83+); Lean method/trait `ensures` sugar still **G-oop** open — [OOP roadmap](2026-05-20-li-oop-roadmap.md)
- [x] Phase 2h — Python-math operators `%`, `//`, `**` (`math_syntax` suite); **`for i in start..<end`** (`for_range_sum.li`); Python `range()` deferred (**G-math-syn**)
- [ ] Phase 2i — Math / linalg surface — **partial:** **2i-a/c** (#148); **2i-b** `norm`, `sum`/`dot`, `reductions/`, same-length `**`, prelude `axpy`, scalar×array; **length-1 broadcast** (`array[1]`→`array[N]` element-wise); full NumPy rank broadcast still open; float `@` Props closed (`mat2_at2_eval`)
- [x] Phase 3 — MIR + LLVM codegen (`lic build`, minimal lower/emit; CFG/bounds IR deferred)
- [x] Phase 4 — Runtime + stdlib
- [x] Phase 4s — Stdlib seal (prelude/`std/` names cannot be shadowed; `stdlib_seal/` CI)
- [x] Phase 5 — Tetris
- [x] Phase 5b — Benchmarks & simulations (harness + **X plots** skeleton on `dev`)
- [x] Phase 6 — Self-host (bootstrap seed: `bootstrap/lic/main.li` → `build/lic-from-li`)
- [x] Phase 2e — Contracts + refinements — **merged (PR #83):** call-site `requires` (**E0304**), refinement types (**E0305**), if-guard VC discharge, import/extern; corpus [proof-corpus-roadmap.md](../verification/proof-corpus-roadmap.md); float/nontrivial ensures still open
- [x] Phase 2f — Lean 4 verify — **partial (#83, #151, #155):** default `lake build AutoVC` on `lic build`; **P-linalg** closed corpus + loop dot (`dot4_int_loop_eval_spec`); fib/recursive call-site + `decreases`/`_par*` VCs typecheck; intentional open: `sqrt_open_bound`; **G-lean** / **G-vc** still open — [still open gaps](../verification/provability-gaps.md#still-open-report-every-session)
- [x] Phase 7 — Native HPC — **v1 gate:** simd + parallel for + OpenMP + `check-master-plan-gates.sh` (tier 1/2 perf advisory)
- [ ] Phase 7d — Execution decorators — **partial (#150 7d-c):** `@vectorized` on `for` → `ArraySimdScope`; **7d-b** lanes=4; **def `@parallel(disjoint=)`** inherits to nested `parallel for` (policy); **open:** full MIR proc tags, Lean **G-par** proofs
- [ ] Phase 7e — Math → SIMD/parallel lowering — **partial (#148, #150, #155):** loop matmul + FMA horner; tier-1 advisory ≤1.2× (`matmul_naive`, `horner_pure_li`); **`check-tier1-li-vs-cpp.sh`** strict optional; **open:** remaining tier-1 slices, full float Lean Props
- [x] Phase H — li-httpd infra — **`lis`** harness, mitigations, CI, workspace stubs ([implementation-status](https://github.com/li-langverse/lis/blob/main/docs/implementation-status.md))
- [x] Phase H — li-httpd M1 `.li` — **partial:** TOML `match_route`, validate/explain/flatten-config, overlap reject, Bearer auth (C), `packages/li-log` (#158); **next:** Li `net.httpd` lib build + M1 ship gate Lean ([httpd-prerequisites](../ecosystem/httpd-prerequisites.md))
- [x] Phase Pkg — Package scaffold + governance stubs ([scaffold](2026-05-16-li-package-scaffold.md), [governance](2026-05-16-li-ecosystem-governance.md); `li.toml` = [lip § A3](2026-05-16-li-package-manager-lip.md))
- [x] Phase 8-repo — [`lic`](https://github.com/li-langverse/lic), [`lip`](https://github.com/li-langverse/lip), [`lit`](https://github.com/li-langverse/lit) on GitHub + CI
- [x] Phase 8a — Modules + workspace `lic build` — `std.*` + workspace/local imports; `li-tests/modules/`; `lic-workspace-build.sh` on 3 packages
- [x] Phase 8e-li — `lic build --coverage-instrument` (LLVM profile flags)
- [x] Phase 8e — `lit` CLI + ≥80% publish gate — **`lit` repo:** `scripts/lit`, coverage gate; **`lic`:** `check-stdlib-coverage.sh` instrumented import harness (`stdlib_coverage/`); llvm-cov 100% on `std/**` in **lit** repo
- [x] Phase 8b — `lip` path/git + `li.lock` — **`lip` repo:** path deps, `install`/`build`/`lock`, integration fixtures
- [x] Phase 8c — ed25519 + `proof_digest` in lock — **v1:** lock fields + optional `publisher.key` signing
- [x] Phase 8d — Registry + `lip publish` — **v1:** local `registry/index.json` + publish gate (`lit` + `lic`)
- [x] Phase 8-sync — cross-repo workflows; optional PAT scope fix for `repository_dispatch`
- [ ] Phase 8p — Parallel compile + CI throughput — **partial (8p-a/b/c/d):** 8p-a parallel `run_all` + isolated `LI_BUILD_DIR` ([#186](https://github.com/li-langverse/lic/pull/186), [#200](https://github.com/li-langverse/lic/pull/200)); 8p-c/d `ResourceOptions` + `lic build --jobs` reserved pass + CI test-job smokes (`compiler/common/`, `scripts/ci.sh`); **open:** 8p-b workspace pool, wall-time SLO ([§ 8p](#phase-8p--parallel-compile--ci-throughput))
- [x] Phase Doc-a — Gap register current + site links ([provability-gaps](../verification/provability-gaps.md))
- [x] Phase Doc-b — Handbook stubs (decorators, linear-algebra); audit partial
- [x] Phase Doc-c — Phase 02 plan links **G-*** IDs (expand to 03/07 as those land)
- [x] Phase Doc-d — Contributing / build skill requires gap-register updates
- [x] Phase Doc-e — `scripts/check-doc-provability-claims.sh` in `scripts/ci.sh` (expand patterns over time)
- [ ] **Vision-LLM** — LLM-first + agent JSON diagnostics — **partial:** `lic check --format=json`, `lic diagnose`, `diagnostic-v1` schema, handover docs, manifest stub ([llm-first spec](../specs/2026-05-16-li-llm-first-design.md), [agent-handover](../ecosystem/agent-handover-formats.md))

**Dashboards (Cursor):** `canvases/li-master-plan-progress.canvas.tsx` — phase tracker; `canvases/li-fuzz-security-dashboard.canvas.tsx` — updated by `scripts/export-fuzz-status.sh` after nightly fuzz.

**Maintenance:** When ecosystem or org layout changes (`lic` / `lip` / `lit` repos, phase gates, policies, **future-repo table**), update **this file**, [2026-05-16-li-package-manager-lip.md](2026-05-16-li-package-manager-lip.md), and [2026-05-16-li-ecosystem-governance.md](2026-05-16-li-ecosystem-governance.md) in the same PR. Agents: add a row to **Future org repos** when a plan introduces a new `li-langverse/*` home.

**Maintenance (proof surface):** When a compiler phase changes what `lic build` proves, update [provability-gaps.md](../verification/provability-gaps.md) and the [compiler task → gap map](#compiler-tasks-vs-proof-gaps) in the **same PR** — see **Doc** phase rules above.

---

## v1 compiler milestone (honest scope)

### Lic monorepo v1 — **gate complete** (`scripts/check-master-plan-gates.sh`)

Runnable on `dev` after `./scripts/build.sh`:

- Phases **0–6**, **2g**, **2h**, **7** (core), **Pkg**, **Doc**, **8-sync**, **8e-li**
- **180** `li-tests` manifest entries (`run_all.sh --ci` after gap-closure)
- **2e/2f partial:** `build/generated/AutoVC.lean` every `lic build`; **P-linalg** closed VCs (#151)
- **7e partial:** 1d/2d float `@`, element-wise SIMD, pure-Li tier-1 `simd_dot` / `matmul_*` (#148)
- **7d partial:** `@vectorized(lanes=4)` + scoped `for` (**#150**); structured `disjoint=` still open

### Full master plan — **not complete** (v2 backlog)

| v2 item | Gap ID(s) | Why still open |
|---------|-----------|----------------|
| **2e–2f** | **G-lean**, **G-vc**, **G-trust** | Kernel discharge; float/loop VCs — [still open](../verification/provability-gaps.md#still-open-report-every-session) · [proof-corpus-roadmap](../verification/proof-corpus-roadmap.md) |
| **2i / 7e** | **G-math** | broadcast, loop-dot proof, remaining tier-1 strict rows |
| **7d** | **G-par**, **G-dec** | Structured `disjoint=`; decorator elaboration |
| **2j proofs** | **G-oop** | Method/trait Lean `ensures` (surface done) |
| **H** | — | M1 ship gate (exploits A+B, li-log, full Lean on server); M1.5 SSE/TLS |
| **8b–8d v2** | — | Remote registry, full trust store |
| **Vision-LLM** | — | Agent JSON diagnostics completion |
| **8p** | — | Parallel `li-tests` / workspace / `lic --jobs`; CI wall-time SLO |
| **Release containers** | — | GHCR images with **prebuilt `lic`** (toolchain-only images exist now; [§ deferred](#deferred--ghcr-release-images-prebuilt-lic)) |

**Open G-* register:** every row in [provability-gaps.md](../verification/provability-gaps.md#still-open-report-every-session) — **none Done**; **Partial** is the best current status.

**“Master plan done”** per original spec = all tracker rows **plus** proved `lic build` **plus** shipped lip/lit/httpd — **not claimed**. Use **Lic monorepo v1** for what ships from this repository today.
