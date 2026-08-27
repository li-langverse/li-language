# Master plan + provability gaps sync (post #151)

## Summary

Updates the phase tracker and adds a standing **Still open (G-*)** section so every session reports which proof gaps remain.

## Agent continuation

1. **Read** [provability-gaps.md](../verification/provability-gaps.md#still-open-report-every-session) before claiming proof or math milestones.
2. **Run** `./li-tests/run_all.sh --ci` (176 pass on `main`).
3. **Next** — close **P-linalg** loop open VC; then **G-lean** default kernel gate.
4. **Blocked on** `Core.lean` array/loop semantics for loop ≡ closed-form proofs.

## Changed

| Path | Note |
|------|------|
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | 2f/2i/7d/7e tracker, v2 backlog, Doc-c, 176 tests |
| `docs/verification/provability-gaps.md` | **Still open** table; G-lean/G-dec/G-oop rows |
| `docs/superpowers/plans/2026-05-14-phase-07-native-hpc.md` | G-* links, 7e/7d status |
| `docs/superpowers/plans/2026-05-16-li-math-linalg-surface.md` | Exit gates + P-linalg |

## Not changed

- Compiler, `li-tests` specimens.

## Breaking

N/A.

## Security / Performance / Downstream

N/A.
