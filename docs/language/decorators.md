# Execution decorators

**Handbook:** [Parallelism](parallelism.md). **Surface:** [execution surface](../superpowers/specs/2026-05-25-li-execution-surface.md).

Li attaches **execution decorators** to `def` and to `for` / `while` loops with `@name` syntax (see [execution decorators spec](../superpowers/specs/2026-05-16-li-execution-decorators.md)).

## Reserved names

Names such as `parallel`, `vectorized`, `async`, `cpu`, and `gpu` are reserved for the standard library. User-defined decorators must use distinct names (minimum length enforced at compile time). `lic check` also flags **typosquats**: a decorator name whose segment is close to a reserved name (e.g. `@my_paralell` vs `@parallel`) warns `W0403` by default, or errors `E0330` when `[check].typosquat = "deny"` is set in `li.toml`.

## Parallel loops

`@parallel` on a `parallel for` loop requires a disjointness proof hint, e.g. `disjoint_elem(i, buf)` in the loop contract list.

## Status

Parsing and policy checks are implemented in `lic check`. MIR elaboration and codegen lowering are tracked as **G-dec** in [provability-gaps](../verification/provability-gaps.md).
