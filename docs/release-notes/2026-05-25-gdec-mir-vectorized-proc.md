# G-dec: `@vectorized` MIR proc tag on `def`

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** PH-7d, G-dec  

## Summary (one sentence)

`@vectorized(lanes=4)` on a `def` lowers to `MirDecorator.vectorized` with resolved `lanes`, exposed as `lic verify mir_vectorized_proc=` telemetry and CI via `check-mir-vectorized-decorator.sh`.

## Agent continuation (required)

1. Read: `compiler/mir/lower.cpp`, `docs/verification/provability-gaps.md` (**G-dec**), `feat/fresh-gpar-7d` for `@parallel` MIR (do not duplicate).
2. Run: `./scripts/build.sh && ./scripts/check-mir-vectorized-decorator.sh`
3. Then: human merge; land `@parallel` MIR from `feat/fresh-gpar-7d` when ready.
4. Blocked on: Lean **P-dec** — not this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| MIR | `MirDecorator.vectorized`, default `lanes=4` | `compiler/mir/include/li/mir.hpp`, `lower.cpp` |
| CLI | `mir_vectorized_proc=` on `lic verify` | `compiler/lic/main.cpp` |
| CI | `check-mir-vectorized-decorator.sh` | `vectorized_dot_proc_ok.li` |

## Not changed (scope fence)

- `@parallel` MIR — **`feat/fresh-gpar-7d`** / **G-par**
- Loop `@vectorized` beyond existing `ArraySimdScope`
- New `decorator_exploits` rows
- Lean **P-dec**

## Breaking changes

None.

## Security

N/A — compile-time metadata; `decorator_exploits/` unchanged.

## Performance

N/A — no new codegen behavior.

## Downstream

N/A
