# Release notes: G-trust Core T-GetElem

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** feat/fresh-g-trust  
**PH / REQ:** PH-2f, G-trust  
**Author:** agent

---

## Summary (one sentence)

Moves **G-trust** to **Partial+** by documenting and proving **T-GetElem** (fixed-size `LiArray` indexing is well-typed when `i < n`) in `docs/semantics/Core.lean`.

## Agent continuation (required)

1. Read: `docs/semantics/Core.lean` § T-GetElem; `docs/verification/provability-gaps.md` **G-trust** row.
2. Run: `(cd docs/semantics && lake build Core)`; with elan: `lake build AutoVC` after a local `lic build` in a specimen tree.
3. Then: add **C-Requires** / **C-Ensures** entailment rules; start `MIR.lean` preservation lemmas for lowering.
4. Blocked on: **G-meta** (full compiler ↔ Lean equivalence) — not this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `docs/semantics/Core.lean` | **T-GetElem** theorem `typing_getElem`; rename build gate to `core_build_ok` | `lake build Core` |
| `docs/verification/provability-gaps.md` | **G-trust** → **Partial+** (summary + register) | Same PR |
| `docs/semantics/README.md` | Core partial slice documented | Links to gaps |

## Not changed (scope fence)

- **MIR.lean** — not created; preservation lemmas open.
- **G-lean** / **G-vc** — AutoVC discharge corpus unchanged; no new `lic build` gate.
- **trusted.lean** — no new axioms.
- **Compiler** — C++ typecheck/MIR lowering unchanged.

## Breaking changes

None.

## Security

N/A — proof-layer documentation only.

## Performance

N/A — Lean semantics library only.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |
