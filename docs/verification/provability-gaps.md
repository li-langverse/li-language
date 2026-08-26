# Provability gaps register

Canonical inventory of **soundness holes** between Li’s north-star (“`lic build` = proof certificate”) and the current compiler. Updated by `proof_gap_researcher` sessions.

| ID | Area | Severity | Status |
|----|------|----------|--------|
| G-VERIFY-01 | Lean gate absent from `lic build` | **Critical** | Open |
| G-VERIFY-02 | `verify_ok` ≡ `compile_ok` in harness | **High** | Open |
| G-SEM-01 | `Core.lean` / `MIR.lean` not present | **High** | Open |
| G-CONTRACT-01 | `requires`/`ensures` presence only | **High** | Open |
| G-CONTRACT-02 | Loop `decreases` / `invariant` not typechecked | **High** | Open |
| G-POLICY-01 | Parallel disjointness via source patterns | **Medium** | Open |
| G-REJECT-01 | `prove_reject` often fails for parse/type errors | **Medium** | Open |
| G-BENCH-01 | Benchmark `verify.py` = `lic build`, not Lean | **Medium** | Open |
| G-CODEGEN-01 | No LLVM↔MIR preservation proof | **Deferred** | Planned |
| G-TRUST-01 | `trusted.lean` only; no `Core` bridge | **Medium** | Open |

## Evidence pointers

See session digest `docs/ecosystem/research-sessions/provability_holes-cycle.md` (session `cf909e52-f0da-4f26-ac10-ff3dbc81b320`, cycle 1).

Reproduction fixture: `li-tests/proof_gaps/false_ensures_still_builds.li`.
