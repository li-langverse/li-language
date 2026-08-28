# Release notes: scalar precision types (float4–float512, int4–int512)

## Summary

The compiler accepts explicit fixed-width scalars (`float32`, `int16`, `float8`, …) with width-mismatch errors, literal suffixes (`3.14f32`, `42i32`), a `binary` type with `0b…` literals, and physics core exposes optional `ScalarPrecision` / profile bit-width metadata without org-wide accuracy enforcement.

## Agent continuation

1. **Read** `docs/language/scalar-precision.md`, `compiler/types/numeric_types.cpp`, `packages/li-physics-core/src/lib.li` (`ScalarPrecision`, `PhysicsProfile.float_bits`).
2. **Run** `./li-tests/run_all.sh typecheck` and `lic check` on packages using new widths.
3. **Then** MIR lowering for `float16`/`float8` packed layouts and `binary` bitwise ops; wire `li.toml` `[numerics]` parser if manifest-driven defaults are needed.
4. **Blocked on** packed `float4`/`float8` memory layout, `binary` codegen, and LLVM types for 128+ bit floats (currently type-check / widen at codegen).

## Changed

| Path | Change |
|------|--------|
| `compiler/types/numeric_types.{hpp,cpp}` | Name → width registry (4…512) |
| `compiler/types/typecheck.cpp` | Width-aware `Int`/`Float`; no silent mix |
| `compiler/types/prelude.cpp` | All scalar widths are prelude-reserved |
| `compiler/mir/lower.cpp` | Float array/dot recognizes new float names |
| `packages/li-physics-core/src/lib.li` | `ScalarPrecision`, profile `float_bits`/`int_bits` |
| `li-tests/typecheck/scalar_width_*.li` | compile_ok / compile_fail |
| `li-tests/typecheck/literal_suffix_ok.li` | Suffix literals |
| `li-tests/typecheck/binary_literal_ok.li` | `binary` + `0b` |
| `std/binary/binary.li` | Facade stub |
| `compiler/lexer/lexer.cpp` | Suffix + `0b` lexing |
| `docs/language/scalar-precision.md` | **Canonical doc** — full tables, suffixes, `binary`, physics API, **You set precision yourself**, agent checklist |
| `packages/li-physics-core/docs/scalar-precision.md` | Package-focused precision guide |
| `std/binary/README.md` | `binary` vs `bytes` |
| `mkdocs.yml`, `language/overview.md`, `AGENTS.md` | Nav + agent pointers |

## Not changed

- Default literals still map to **64-bit** (`int` / `float`).
- No org CI rule mandating `float64` or `float32`.
- Full quantization kernels and `float4` layout in MIR/LLVM.
- E0303 strict ensures (separate PR #70).

## Breaking

N/A — additive surface types; existing `int`/`float` unchanged.

## Security

N/A.

## Performance

N/A at typecheck layer; `float32` codegen uses LLVM `float` when lowering applies.

## Downstream

Package mirrors: document chosen widths in README; optional `[numerics]` in `li.toml` per package.
