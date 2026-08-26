# Release notes: 2026-05-25 — p-refine-call-site-closed

**Summary:** `refinement_call_ok.li` → `prove_lean_ok`; real refine Props via `Li.Discharge.refinement_nonneg_spec`.

## Agent continuation

1. Read `Discharge.lean` (`refinement_nonneg_*`).
2. Run `./li-tests/tooling/discharge_refinement_lean.sh`.
3. Then init/guard refinements; **P-float** on `feat/fresh-p-float` only.
4. Blocked: non-literal refine; **G-lean** Done.

## Not changed

**P-float** (`feat/fresh-p-float`, dedupe #185); other `refinement_*`; **P-ensures-witness**.
