# CallProc — array parameters by pointer

## Summary

`def f(a: array[N, float])` and object-flattened array fields pass allocas as `ptr to [N x T]` at call sites; callees copy into local array slots.

## Agent continuation

1. **Read** `MirArg.is_array_ident`, `llvm_array_param_ptr`, `vectorized_dot_proc_ok.li`.
2. **Run** `./li-tests/run_all.sh` (**171** pass).
3. **Then** merge **#148**; loop `@vectorized` elaboration.
4. **Blocked on** by-value array returns across calls.

## Changed

- `compiler/mir/`, `compiler/codegen/emit.cpp`
- `li-tests/decorators/vectorized_dot_proc_ok.li`

## Not changed

- Heap-dynamic arrays; slice params.

## Breaking

N/A.

## Security

N/A.

## Performance

N/A.

## Downstream

- Math helpers can take `array[N, float]` parameters.
