# MIR lowering for `object` field access and expanded parameters

## Summary

`object`-typed values are lowered as flattened per-field locals (`__li_o_<root>_<field>…`) for field access, assignment, `var`, expanded call arguments, same-type `var dst: T = srcIdent` slot copies, whole-object assigns, **element-wise copy of fixed `array[N, int|float]` fields**, and **LLVM struct returns** that pack scalars **and** fixed arrays as `[N x T]` members (`ReturnObject`, callee unpack, `var w: T = foo()`).

## Agent continuation

1. Read `compiler/mir/include/li/mir.hpp` (`ReturnObject`, `MirFn::returns_object` / `return_object_layout`, `MirInsn::object_layout`), `compiler/mir/lower.cpp`, `compiler/codegen/emit.cpp`.
2. Run `cmake --build build` then `./li-tests/run_all.sh encapsulation` and `./li-tests/run_all.sh math_linalg` with `LIC` pointing at the built `lic`.
3. Next: `array` fields with `i64` / other scalar elems in struct returns; `extern proc` returning object; `typedict` parity.
4. Blocked: `array` fields with element types other than `int` / `float` (e.g. `i64`) on object return/param paths; nested `extern` object ABI.

## Changed

- `compiler/mir/include/li/mir.hpp` — `MirParam::fixed_array_elems`; comments for `ReturnObject` / `object_layout`.
- `compiler/mir/lower.cpp` — `collect_object_return_layout_r` / `append_mir_params_for_object_type` include fixed `array[N, int|float]`; implicit `ReturnObject` zero uses per-index `ArrayStore*` for those fields; `seed_*_params` skip fixed-array param roots; prior object work unchanged (`ReturnObject`, `CallProc` temps, `var`/`Assign` copies, `emit_copy_array_slots_r`, `g_arr_ctx`, `Index` / `Assign` `FieldAccess` bases, deferred `g_arr_ctx` clear).
- `compiler/codegen/emit.cpp` — `llvm_type_for_mir_param`, LLVM struct fields `[N x T]`, `ReturnObject` aggregate load, `CallProc` aggregate store to `ArrayAlloc` slots, `mir_arg_value` loads for array idents, prologue stores for expanded array params.
- `li-tests/objects/object_array_return_call.li` — callee returns object whose only field is `array[2, int]`.
- `li-tests/objects/object_mixed_scalar_array_return.li` — struct return with `int`, `array[2, int]`, `int` fields (ordering vs layout).
- `li-tests/objects/object_mixed_param_pass.li` — expanded `Mixed` object parameter (scalar + `array[2,int]`) passed to callee.
- `li-tests/manifest.toml` — register `object_mixed_param_pass.li`.

## Not changed

- General scalar/control-flow codegen paths in `emit.cpp` beyond struct-return / aggregate-array plumbing.
- `typedict` runtime layout (still not flattened in MIR).
- Parser, borrow checker, and verifier contracts for objects.
- Dominance: `object_locals` is a flat map from the whole procedure AST; assigning to a name that is not object-typed or not declared on all paths may still be accepted by the typechecker separately from MIR slot layout.
- `extern proc` returning a Li `object` type (no struct lowering contract).
- `return_object_layout` / LLVM struct fields for `array` elements other than `int` / `float` (e.g. `i64`) remain unsupported.

## Breaking

N/A — fixes incorrect codegen for programs that already type-checked.

## Security

N/A.

## Performance

N/A — same stack locals as before for scalar `object` fields; more MIR parameters for object-by-value calls (ABI expansion).

## Downstream

Callers of procs that take `object` parameters must pass the same Li source (compiler expands arguments); no C ABI change for non-object programs.
