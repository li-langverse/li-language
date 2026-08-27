# PH-HW WP4 — lig.kernel + LKIR

## Summary
packages/lig LKIR matmul pilot, lig.kernel.run, li_rt_lig.c.

## Agent continuation
1. Read packages/lig/src/lib.li
2. Run lic build packages/lig/li-tests/smoke/kernel_matmul_parity.li; ./scripts/bench-lig-kernel-parity.sh
3. WP3 present; CUDA/HIP emit
4. Blocked: device proofs

## Changed
packages/lig, runtime/li_rt_lig.c, bench script, composable hooks

## Not changed
li-gpu rename, real vendor codegen, Studio viewport

## Breaking / Security / Performance / Downstream
N/A / N/A / local JSON / feat/ph-hw-multi-vendor
