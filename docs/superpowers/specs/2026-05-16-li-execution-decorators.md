# Execution decorators (spec stub)

**Status:** Phase 7d in progress (parse landed) — **not** a proof or lowering gate yet  
**Plan:** `.cursor/plans/li_execution_decorators_7c6e3b42.plan.md`  
**Gaps:** [Provability gaps](../../verification/provability-gaps.md) **G-dec**

## No runtime semantics

Decorators are **not** Python-style runtime callables. There is:

- **No** runtime decorator registry or dynamic dispatch  
- **No** wrapping functions at call time  
- **No** interpreting `@foo` when the program runs  

`@parallel`, `@vectorized`, `@cpu`, etc. are resolved entirely during **`lic build`**: parse → validate → elaborate to MIR (`ParallelFor`, SIMD lanes, device tags). Illegal stacks or missing `disjoint=` are **compile errors**.

User `decorator def` macros expand at **compile time** to a whitelist of builtin decorators only.

**Goal:** zero user-visible decorator failures at runtime — if it builds, placement and parallelism shape are already proved.

---

Reserved stdlib names: `cpu`, `gpu`, `tpu`, `user_defined`, `parallel`, `vectorized`, `async`, `serial`, `no_vectorize`.

User `decorator def` names: strict package-prefixed snake_case; typosquat ban; expansion whitelist to builtins only.

See master plan Phase **7d** and `li-tests/decorator_exploits/` (to land with 7d-e).
