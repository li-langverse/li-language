# Proof-gap digest — session `cf909e52-f0da-4f26-ac10-ff3dbc81b320` (cycle 1)

**Agent:** `proof_gap_researcher` · **Goal:** `provability_holes` · **north_star_fit:** ecosystem · **PH-2e, PH-2f**  
**Completed steps:** `read_register-1`, `contract_tier-2`, `synthesize_step` (proof-gap digest)  
**Repo:** `lic` (local: `/Users/julian/Documents/coding-projects/li`)

---

## Executive summary

- **`lic build` is not a proof certificate today** — frontend is parse + C++ typecheck + LLVM emit; no Lean invocation (`compiler/lic/main.cpp:57-81`).
- **`verify_ok` tests are mislabeled** — `li-tests/run_all.sh:61-68` treats `verify_ok` identically to `compile_ok` (both call `lic build` only).
- **Formal semantics incomplete** — `docs/semantics/README.md:10-11` lists `Core.lean` / `MIR.lean` as *planned*; only `trusted.lean` exists.
- **Contract tier is syntactic** — `typecheck.cpp:557-572` enforces `requires`/`ensures` *presence*, not discharge; false postconditions compile (`li-tests/proof_gaps/false_ensures_still_builds.li`).
- **Parallel safety is policy heuristics** — `policy.cpp:22-60` rejects races via substring patterns (`disjoint_row`, `grid[0][0]`, etc.), not proved obligations.
- **`prove_reject` suite passes but often for the wrong reason** — e.g. `uses_sorry.li` fails parse (`expected '='`), not a `sorry` ban; `bare_cast.li` fails on unknown `cast` identifier.
- **Benchmark “verify” is build smoke** — `benchmarks/harness/verify.py:18-31` only runs `lic build` on tier-0 `.li` files.
- **Trusted surface is small and appropriate** — `docs/semantics/trusted.lean:8-18` (IO axioms only); no edits made this session.

---

## 1. Compiler / semantics gaps

| Gap | Evidence | Repro |
|-----|----------|-------|
| No Lean in compiler driver | `compiler/lic/main.cpp:17-24` — subcommands `parse`, `check`, `build`; no proof phase | `lic build li-tests/contracts_verify/sqrt_contract.li -o /dev/null` → exit 0 |
| `lic check` = typecheck only | `main.cpp:57-65` — `check_file` calls `frontend` only | `lic check <file>` same gates as build frontend |
| Missing `Core.lean` / `MIR.lean` | `docs/semantics/README.md:10-11` | `find docs/semantics -name 'Core.lean'` → absent |
| Codegen without preservation proof | `docs/verification/overview.md:47` admits meta-proof is future work | N/A (deferred G-CODEGEN-01) |

---

## 2. Contract gaps

| Gap | Evidence | Repro |
|-----|----------|-------|
| Ensures not verified | `sqrt_contract.li:7-8` stub body `return x` with strong `ensures` | `lic build li-tests/contracts_verify/sqrt_contract.li` → exit 0 |
| False postcondition compiles | New fixture | `lic build li-tests/proof_gaps/false_ensures_still_builds.li -o /tmp/t` → exit 0, binary emitted |
| Loop totality not enforced | `missing_decreases.li` fails parse on `while`, not `decreases` check | `lic build li-tests/prove_reject/missing_decreases.li` → parse errors, not “missing decreases” |
| Refinement types parsed, not proved | `index_refinement.li` — `Index10` alias; no Lean | passes `verify_ok` via build-only harness |

**Contract tier (session step 2):** Spec mandates kernel discharge (`2026-05-14-li-language-design.md:682-697`). Implementation tier today = **Tier A (syntax)** only: presence of `requires`/`ensures` on non-`extern` procs (`typecheck.cpp:567-572`). **Tier B (VC discharge)** and **Tier C (refinement proof)** are documented but not implemented.

---

## 3. Trusted surface

| Item | Location | Notes |
|------|----------|-------|
| IO monad + frame/event axioms | `docs/semantics/trusted.lean:8-18` | Minimal; RFC-governed per spec |
| `extern proc` skips body check | `typecheck.cpp:549-551` | Trusted boundary; must align with `trusted.lean` listings |
| Planned alloc laws | Spec `2026-05-14-li-language-design.md:555,717` | Not in current `trusted.lean` |

**Session rule:** No `trusted.lean` edits (human-approved issues only).

---

## 4. External trust boundaries (human decision if outside lic)

| Boundary | Trust assumption | Owner |
|----------|------------------|-------|
| LLVM 18 codegen | Correct lowering until translation validation | Platform / compiler team |
| C reference kernels in benchmarks | Shared physics kernel for tier-2 ratio tests | Benchmarks harness |
| `lake build` / Mathlib | Toolchain for future `Core.lean` | Semantics WG |
| GitHub CI / agent swarm | Process, not soundness | Ecosystem |

---

## 5. Evidence pack

### Commands run (2026-05-18)

```bash
cd /Users/julian/Documents/coding-projects/li
cmake --build build                                    # ok
./li-tests/run_all.sh prove_reject                     # pass=5 fail=0
./li-tests/run_all.sh contracts_verify                 # pass=2 fail=0
./li-tests/run_all.sh race_shared_memory               # pass=7 fail=0
lic build li-tests/proof_gaps/false_ensures_still_builds.li -o /tmp/li_gap_test  # exit=0 (GAP)
lic build li-tests/prove_reject/uses_sorry.li          # parse error, not sorry ban
```

### Hypothesis outcomes

- `HYPOTHESIS: verified — lic build does not invoke Lean | evidence: compiler/lic/main.cpp:57-81 (frontend only)`
- `HYPOTHESIS: verified — verify_ok is equivalent to compile_ok in harness | evidence: li-tests/run_all.sh:61-68`
- `HYPOTHESIS: verified — false ensures still produce binaries | evidence: lic build li-tests/proof_gaps/false_ensures_still_builds.li → exit 0`
- `HYPOTHESIS: falsified — prove_reject/uses_sorry.li is rejected because sorry is banned | evidence: parse error expected '=' at body (proof/sorry syntax unsupported)`
- `HYPOTHESIS: falsified — prove_reject/bare_cast.li is rejected because bare cast is forbidden | evidence: unknown variable 'cast' (cast syntax not implemented)`
- `HYPOTHESIS: verified — parallel race tests are enforced via policy.cpp heuristics | evidence: policy.cpp:37-60 + race suite pass`
- `HYPOTHESIS: deferred — LLVM↔MIR preservation | evidence: Core.lean/MIR.lean absent; spec marks future work`

### New artifacts

- `docs/verification/provability-gaps.md` — gap register (G-* IDs)
- `li-tests/proof_gaps/false_ensures_still_builds.li` — G-VERIFY-01 repro
- `li-tests/proof_gaps/README.md` — manual run instructions

---

## Recommended issues/PRs

| Repo | Title | Labels (suggested) |
|------|-------|-------------------|
| `lic` | `feat(verify): wire Lean 4 discharge into lic build (G-VERIFY-01)` | `pillar:provable`, `PH-2f` |
| `lic` | `test(harness): distinguish verify_ok from compile_ok; fail on proof gaps` | `pillar:provable`, `testing` |
| `lic` | `feat(semantics): add Core.lean typing + contract semantics (G-SEM-01)` | `pillar:provable`, `PH-2e` |
| `lic` | `feat(typecheck): enforce loop decreases/invariant (G-CONTRACT-02)` | `pillar:provable` |
| `lic` | `fix(prove_reject): reject sorry/cast/decreases with semantic diagnostics (G-REJECT-01)` | `pillar:provable`, `good-first-issue` |
| `lic` | `docs(verification): keep provability-gaps.md in sync with sessions` | `documentation` |

---

## Deferred

- Translation validation (LLVM ↔ MIR ↔ Lean preservation) — blocked on `MIR.lean` + codegen spec.
- Coq export path — spec Phase 7+.
- Alloc effect axioms in `trusted.lean` — spec mentions; not yet required by compiler.
- Enrolling `proof_gaps/` fixtures in CI — after Lean gate exists (otherwise intentional red CI).

---

## Error

None this session.
