# Release notes: manifest prove_lean_ok (G-test-verify)

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** feat/g-items-wave  
**PH / REQ:** PH-2f, G-test-verify  
**Author:** agent

---

## Summary (one sentence)

Closes **G-test-verify** by adding `prove_lean_ok` to `li-tests/run_all.sh` and retagging 13 closed `contracts_verify` specimens so CI distinguishes strict compile from Lean+AutoVC discharge.

## Agent continuation (required)

1. Read: `docs/verification/proof-corpus-roadmap.md` § G-test-verify; `li-tests/run_all.sh` `prove_lean_ok` branch.
2. Run: `LI_REPO_ROOT=$PWD ./li-tests/run_all.sh contracts_verify` and `./li-tests/tooling/contracts_discharge_corpus.sh`; with elan: `command -v lake && (cd docs/semantics && lake build AutoVC)`.
3. Then: retag remaining `verify_ok` rows when `discharge_*_lean.sh` exists; wire CI semantics job with elan so `prove_lean_ok` does not skip.
4. Blocked on: **P-float** (`sqrt_open_bound`), **P-refine**, **P-ensures-witness** (non-literal returns), **G-par** / **G-dec** — not this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `li-tests/run_all.sh` | `prove_lean_ok`: build + `check-autovc-open-goals.sh` + `lake build AutoVC` | New outcome branch |
| `li-tests/manifest.toml` | 13 `contracts_verify` → `prove_lean_ok` | P-linalg + discharge scripts |
| `docs/verification/provability-gaps.md` | **G-test-verify** → **Done** | Gap register row |
| `docs/verification/proof-corpus-roadmap.md` | Manifest outcome docs + run results | No overclaim on `verify_ok` |
| `docs/verification/overview.md` | `prove_lean_ok` row in proof gate table | Manifest honesty |
| `li-tests/tooling/contracts_discharge_corpus.sh` | Caller-requires discharge in corpus gate | P-ensures call-site partial |

## Not changed (scope fence)

- **G-lean** / **G-vc** / **G-trust** — Lean kernel still not universal proof certificate on every `lic build`.
- **P-float** — `sqrt_open_bound.li` remains `verify_open_ok`; `Li.Discharge.sqrt_open_bound_placeholder` only.
- **P-refine** — refinement VCs still stubbed; `refinement_*` stay `verify_ok`.
- **P-ensures-witness** — `sqrt_contract.li` / method-call specimens not retagged (no full MIR witness proof).
- **G-par**, **G-dec**, **G-meta**, master plan tracker checkboxes — other agents.
- **lip** / **lit** / **li-cursor-agents** — no cross-repo pins.

## Breaking changes

None.

## Security

N/A — test harness only; no new attack surface.

## Performance

N/A — `prove_lean_ok` adds lake when installed; skipped without elan.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |

## CHANGELOG entry (paste into Unreleased)

- **G-test-verify Done:** `prove_lean_ok` manifest outcome + 13 closed `contracts_verify` rows — `docs/release-notes/2026-05-25-g-test-verify-prove-lean-ok.md`.
