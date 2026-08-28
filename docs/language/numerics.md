# Numerics

Li uses **Python-like names** with **compiled fixed-width** behavior. This catches common scientific bugs (like adding an `int` to a `float` by accident).

## Default mappings

| You write | Machine |
|-----------|---------|
| `int` | 64-bit signed |
| `float` | 64-bit IEEE |
| `42` | integer literal → `i64` |
| `3.14` | float literal → `f64` |

**Full width tables, suffixes, `binary`, and physics metadata:** [Scalar precision](scalar-precision.md) (canonical).

**You choose precision** — explicit types (`float32`), suffixes (`3.14f32`), optional `li.toml` `[numerics]`, and physics `float_bits` are all **per-project / per-module** choices; the org does not enforce one global width. See [You set precision yourself](scalar-precision.md#you-set-precision-yourself).

## Literal suffixes

```
42       # int (i64)
42u      # uint (u64)
42i32    # int32
255u8    # uint8
3.14     # float64
3.14f32  # float32
1.0f16   # float16
0b1011   # binary
2.0 + 1.0i   # complex
```

## Operators (important rules)

| Expression | Result |
|------------|--------|
| `int + int` | `int` |
| `float + float` | `float` |
| `int + float` | **Error** (must cast explicitly) |
| `int / int` | `float` (Python 3 division) |
| `int // int` | floor division |

## Overflow

Default integers are **checked**: overflow must be impossible to prove, or you use an explicit mode:

| Mode | Meaning |
|------|---------|
| `checked int` | Default — must prove no overflow |
| `wrapping i32` | Modular arithmetic with proof |
| `saturating i32` | Clamped arithmetic with proof |

There is **no** silent `unchecked int`.

## Effects on numeric ops

| Situation | Effect |
|-----------|--------|
| Division by zero | `raises DivZero` or compile error if divisor is literal 0 |
| `sqrt` of negative | `raises Float` (by default) |

## SIMD numerics

`simd[T, N]` supports lane-wise `+`, `*`, and intrinsics such as horizontal sum. Element type `T` is typically `f32` or `f64`.

See [SIMD and parallel](simd-parallel.md).

## Roadmap (not all shipped yet)

| Phase | Features |
|-------|----------|
| v1 | Scalars, complex, SIMD, parallel CPU |
| v2 | `f16`, `bf16`, async generators |
| v3 | `tensor[Shape, T]`, GPU buffers |

Full tables: [design spec — numeric roadmap](../superpowers/specs/2026-05-14-li-language-design.md#numeric-roadmap).
