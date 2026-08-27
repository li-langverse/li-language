# Scalar precision in `physics.core`

This package does **not** enforce a global float width for the Li org. It exposes **metadata** so simulations, games, and agents can record which scalar widths and weight encodings a profile uses.

**You set precision** in source (`float32`, `type Real = …`, suffixes) and optionally on `PhysicsProfile.float_bits` / `ScalarPrecision` presets — see [You set precision yourself](https://github.com/li-langverse/lic/blob/main/docs/language/scalar-precision.md#you-set-precision-yourself) in the language handbook.

**Canonical language doc:** [Scalar precision, literals, and binary data](https://github.com/li-langverse/lic/blob/main/docs/language/scalar-precision.md) (in the `lic` repo).

## Types

### `ScalarPrecision`

| Field | Values | Meaning |
|-------|--------|---------|
| `float_bits` | 4, 8, 16, 32, 64, 128, … | Preferred float width for this preset |
| `int_bits` | same | Preferred int width |
| `weights_encoding` | `0` or `1` | `0` = float tensor weights; `1` = **binary** packed weights |

### `PhysicsProfile`

Includes `float_bits` and `int_bits` alongside `tier`, `dt`, `substeps`, and `targets`. Defaults to 64/64 in `default_physics_profile()`.

## Presets (`src/lib.li`)

| API | Use when |
|-----|----------|
| `precision_default()` | Production FP64 simulation |
| `precision_float32()` | Faster / GPU-friendly FP32 path |
| `precision_quantized_fp8()` | FP8 quantization experiments |
| `precision_binary_weights()` | Bit-packed weight planes (`binary` type in source) |

## Example

```nim
def use_fp32_profile() -> PhysicsProfile
  requires true
  ensures result.float_bits == 32
  decreases 0
=
  var p: PhysicsProfile = default_physics_profile()
  p.float_bits = 32
  p.int_bits = 32
  return p
```

## Import

```nim
import physics.core
```

Workspace package: `packages/li-physics-core` (`import_name` in `packages/li.toml`).

## Precision-polymorphic APIs

Shared math/physics should use **generics** or a **`type Real = …` alias** so every width stays applicable:

- Guide: [Precision polymorphism](https://github.com/li-langverse/lic/blob/main/docs/language/precision-polymorphism.md)
- Example test: `li-tests/generics/precision_polymorphic.li`

```nim
type Real = float32

def step(vx: Real, fz: Real, dt: Real) -> Real
  ensures result == vx + fz * dt
  decreases 0
=
  return vx + fz * dt
```

## Not in scope

- Compiler does not read `float_bits` to change LLVM types automatically (yet).
- Downstream physics packages (`physics.rigid`, `physics.runtime`, …) still use `float` in signatures until width-generic APIs land — set profile metadata first, then migrate kernels per [precision-polymorphism.md](https://github.com/li-langverse/lic/blob/main/docs/language/precision-polymorphism.md).
