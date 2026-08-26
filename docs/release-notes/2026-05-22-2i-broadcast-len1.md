# 2i: length-1 array broadcast (element-wise)

## Summary

Element-wise `+ - * / **` on 1d arrays accepts `array[1, T]` broadcast to `array[N, T]` (float and int); codegen reuses index 0 of the shorter operand.

## Agent continuation

1. **Read:** `compiler/types/typecheck.cpp` (BinOp element-wise), `compiler/mir/lower.cpp` (`ArrayBinOpF64`), `compiler/codegen/emit.cpp`.
2. **Run:** `lic build li-tests/math_linalg/broadcast_len1_*.li`; full `li-tests` slice.
3. **Next:** general NumPy-style rank rules; P-float `sqrt_open_bound`.
4. **Blocked on:** 2d/nd broadcast; SIMD gather for broadcast rhs in vectorized loops.

## Changed

| Path | Evidence |
|------|----------|
| `compiler/types/typecheck.cpp` | `array[1]` × `array[N]` typecheck |
| `compiler/mir/*`, `compiler/codegen/emit.cpp` | `array_broadcast_*_len1` MIR + LLVM |
| `li-tests/math_linalg/broadcast_len1_*.li` | add/mul smoke |

## Not changed

- `sqrt_open_bound.li` intentional open specimen.
- Full multidimensional broadcast.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A — new accepted programs only |
| **Security** | N/A |
| **Performance** | Scalar-loop broadcast (no SIMD tile yet) |
| **Downstream** | N/A |
