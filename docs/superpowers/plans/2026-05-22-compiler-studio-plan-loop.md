---
name: Compiler + Studio plan loop
overview: Autonomous loop — Wave A compiler milestones (2e/2f, 2i, 7d/7e) first, then sim/physics/chem/bio/Studio. Not httpd.
todos:
  - id: wave-a-7e-verify
    content: "Tier-1 result verify — reference.py + bench-verify-results.sh 1; all checksums pass"
    status: pending
  - id: wave-a-7e-pure-li-simd
    content: "Pure-Li simd_dot — correct spec checksum at realistic wall time (DCE allowed if verified)"
    status: pending
  - id: wave-a-7e-matmul-pure
    content: "Pure-Li matmul_naive/matmul_blocked — maintain verify + perf row honesty"
    status: in_progress
  - id: wave-a-7e-horner
    content: "horner_pure_li — volatile sink + reference; no unverified fast-math DCE"
    status: completed
  - id: wave-a-2f-lean-strict
    content: "G-lean — autovc_lake_typecheck + glean_strict_build_smoke green on lic build"
    status: pending
  - id: wave-a-2f-vc-corpus
    content: "contracts_discharge_corpus + contracts_verify_lean; close sqrt_open_bound or document"
    status: pending
  - id: wave-a-2i-explicit-math
    content: "2i — explicit math only (dot, matmul, sum, element-wise on matching shapes); reject NumPy broadcast; document tensor/quaternion path"
    status: pending
  - id: wave-a-7d-vectorized
    content: "7d — @vectorized on def + Lean G-par disjoint roadmap note"
    status: pending
  - id: wave-b-tier2-verify
    content: "Tier-2 physics — verify.py green on md_lennard_jones + one PDE smoke"
    status: pending
  - id: wave-b-md-oracle
    content: "md_lennard_jones — external oracle column plan in competitive-engines (doc + stub driver)"
    status: pending
  - id: studio-math-goldens
    content: "verify-math-physics-goldens.sh in CI gates; li-math golden procs documented"
    status: completed
  - id: studio-algo-wave-a-doc
    content: "algorithms-and-libraries-plan Wave A table synced to provability-gaps"
    status: pending
  - id: studio-ph-ux-slice
    content: "PH-UX — one adaptive layout composable + li-tests compile_ok (no httpd)"
    status: pending
isProject: false
---

# Compiler + Studio autonomous loop

**Not in scope:** `li-httpd`, tier5 HTTP — use `httpd-plan-loop.py` in a separate process.

**Priority:** [master plan](2026-05-14-li-master-plan.md) open **2e/2f/2i/7d/7e** → [algorithms-and-libraries-plan](../../ecosystem/algorithms-and-libraries-plan.md) Wave B/C.

**Philosophy:** [li-benchmark-correctness.mdc](../../../.cursor/rules/li-benchmark-correctness.mdc) — correct per spec, fast as possible; **DCE allowed, our harness must verify**.

**Math surface:** vector algebra, matrices, quaternions, tensors — **explicit** operations only. **No NumPy-style broadcasting** (no silent shape promotion); mismatched shapes are compile errors unless written out explicitly in code.

**Loop:** `./scripts/compiler-studio-plan-loop.py` · gates: `./scripts/compiler-studio-plan-gates.sh`
