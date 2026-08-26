# Compiler E0360: extern pointer-width ABI guard

## Summary

`lic` rejects MIR where `extern proc -> ptr` (or `CallExtern` to such symbols) would store or return a truncated i32, via `verify_mir_extern_abi` before LLVM emit (`E0360`).

## Agent continuation

1. **Read** `compiler/mir/mir_abi.cpp`, `compiler/codegen/compile.cpp` (post-`lower_to_mir` call), `li-tests/runtime/argv_ptr_abi.li`.
2. **Run** `cmake --build build --target lic` and `li-tests/run_all.sh` suite `runtime` from repo root with `LI_REPO_ROOT=$PWD`.
3. **Then** merge stacked PR on `cursor/extern-ptr-abi-guard-54aa` after `cursor/httpd-proxy-bench-fix-54aa`; rebuild `li-httpd` and refresh benchmarks matrix.
4. **Blocked on** human merge of proxy PR #153; no automated `merge-approved` on this branch.

## Changed

| Area | Paths / evidence |
|------|------------------|
| MIR verify | `compiler/mir/mir_abi.cpp`, `compiler/mir/include/li/mir_abi.hpp` — extern `returns_i64`, `CallExtern`/`CallProc` `is_i64`/`ret_is_i64` |
| Compile gate | `compiler/codegen/compile.cpp` — fail build with `E0360` message before `emit_llvm_ir` |
| Codegen | `compiler/codegen/emit.cpp` — Li proc `returns_i64` → LLVM `i8*` ret; `ReturnIdent` widens ptr returns |
| Lower | `compiler/mir/lower.cpp` — `CallExtern` sets `ret_is_i64`; return paths tag `ret_is_i64` |
| Diagnostics | `compiler/diagnostics/include/li/error_codes.hpp` — `E0360` |
| Tests | `li-tests/runtime/argv_ptr_abi.li`, `li-tests/manifest.toml` |

## Not changed

- `trusted.lean` / Lean VC discharge scripts.
- `std/runtime/seam.li` symbol list (proxy PR).
- Tier-5 harness oracle configs in `benchmarks`.
- Typechecker rules for `ptr` vs `int` (still `E0202` at type level only).

## Breaking

N/A — stricter compile-time check; correct programs unchanged.

## Security

N/A — prevents silent UB from ABI truncation, not a new CVE class test.

## Performance

N/A — verify pass is O(MIR insns) at compile time only.

## Downstream

- **benchmarks** — no catalog change; consume lic builds that include this guard.
- **li-httpd** — rebuild after both proxy + guard PRs land on `main`.
