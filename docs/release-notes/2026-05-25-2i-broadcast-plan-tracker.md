# 2i: length-1 broadcast plan tracker + reduction shape specimens

## Summary

Close the **G-math** length-1 broadcast slice in `li-tests/math_linalg/` (add `pow`, explicit `array[2]*array[4]` compile_fail) and restore reduction shape compile_fail files; update master plan **2i** tracker. Supersedes `feat/2i-linalg-slice`; tier-1 reporter work remains on `feat/fresh-7e-tier1`.

## Agent continuation

1. **Read:** `docs/superpowers/plans/2026-05-14-li-master-plan.md` (Phase **2i**), `docs/verification/provability-gaps.md` (**G-math**).
2. **Run:** `./li-tests/run_all.sh math_linalg`.
3. **Next:** general NumPy rank broadcast; **7e** tier-1 smoke from `feat/fresh-7e-tier1`.
4. **Blocked on:** 2d/nd broadcast; SIMD broadcast tiles.

## Changed

| Path | Evidence |
|------|----------|
| `li-tests/math_linalg/broadcast_len1_pow_int4.li` | `**` length-1 broadcast |
| `li-tests/math_linalg/broadcast_invalid_len2_vs_len4.li` | compile_fail |
| `li-tests/math_linalg/reductions/{sum_non_array,dot_len_mismatch}.li` | shape errors |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | **2i** tracker |
| `docs/verification/provability-gaps.md` | **G-math** |

## Not changed

- Compiler broadcast MIR/codegen (on `main`).
- Tier-1 smoke CSV (`feat/fresh-7e-tier1`).
- Full NumPy rank broadcast.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A |
| **Security** | N/A |
| **Performance** | N/A |
| **Downstream** | Supersedes `feat/2i-linalg-slice` |
