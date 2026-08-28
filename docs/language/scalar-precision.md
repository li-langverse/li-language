# Scalar precision, literals, and binary data

**Status:** Shipped in `lic` typechecker (width rules, suffixes, `binary` literals). MIR/codegen for packed `float4`/`float8` and full `binary` runtime is **partial** — see [Codegen maturity](#codegen-maturity) below.

Li exposes **explicit** integer and float widths for HPC, quantization, and per-domain accuracy. The org **does not** enforce one global float width — each package and simulation chooses.

**Normative registry (compiler):** `compiler/types/numeric_types.cpp`  
**Physics metadata API:** `packages/li-physics-core/src/lib.li` (`ScalarPrecision`, `PhysicsProfile`)

---

## You set precision yourself

**You** (package author, game dev, simulation author) choose scalar width — not the Li org, not a parent `li.toml`, and not `physics.core` defaults alone. Pick the accuracy that fits **your** module; other packages in the same repo may use different widths.

| How you set it | What it affects | Enforced on others? |
|----------------|-----------------|---------------------|
| **Explicit types** — `var x: float32`, `def f(v: float16) -> float16` | Types and width-mix errors in **your** source | No |
| **Module alias** — `type Real = float32` at top of file | Whole file uses one width (Pattern A) | No |
| **Literal suffixes** — `1.0f32`, `42i32`, `0b1010` | Inferred type of each literal | No |
| **`li.toml` `[numerics]`** — `default_float = "float32"` | Documents intent for agents/humans | **No** — not applied to dependencies |
| **Physics profile** — `p.float_bits = 32`, `precision_float32()` | Runtime metadata for kernel/asset choice | No — metadata only today |
| **Separate modules** — e.g. `integrator_fp32.li` vs `integrator_fp64.li` | Ship two builds or two entry points | No |

Nothing in the toolchain **forces** your project to `float64` or `float32`. Defaults (`float`, `int`, unsuffixed `3.14` / `42`) are **64-bit** for ergonomics; narrow them explicitly when you want less precision or smaller storage.

### End-to-end example (FP32 simulation)

```nim
# Your package — you chose float32 everywhere in this module
type Real = float32

def step(dt: Real) -> Real
  requires dt > 0.0f32
  ensures result >= 0.0f32
  decreases 0
=
  var t: Real = 0.0f32
  return t + dt

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var dt: Real = 0.016f32
  var _ = step(dt)
  return 0
```

Optional manifest (documents your choice; does not rewrite dependent code):

```toml
[numerics]
default_float = "float32"
default_int = "int32"
```

Optional physics metadata (records width for dispatch / assets):

```nim
var p: PhysicsProfile = profile_for_tier(physics_tier_arcade())
p.float_bits = 32
p.int_bits = 32
```

For shared math at **multiple** widths in one codebase, see [Precision polymorphism](precision-polymorphism.md) (generics `[S]`, duplicate modules per `Real`, proposed `precision float32:` block).

---

## Design principle: no global accuracy

| Do | Do not |
|----|--------|
| Pick `float32` or `float16` in **your** module | Assume all of `li-langverse` uses the same width |
| Set `PhysicsProfile.float_bits` per game/sim | Add CI that rejects `float64` in physics packages |
| Document `[numerics]` in **your** `li.toml` | Override dependent packages’ types from a parent manifest |

See [Strict by default](../ecosystem/strict-by-default.md) — gates apply to proof/security/perf, **not** to forcing `float64` everywhere.

---

## Float types (complete list)

| Type names (examples) | Bits | Notes |
|----------------------|------|--------|
| `float4`, `Float4`, `f4` | 4 | Logical quantized lane; packed layout TBD |
| `float8`, `Float8`, `f8` | 8 | Logical FP8 / quantized (E4M3, E5M2 — **package policy**) |
| `float16`, `Float16`, `f16` | 16 | IEEE binary16 |
| `float32`, `Float32`, `f32` | 32 | IEEE binary32 |
| `float64`, `Float64`, `f64`, **`float`** | 64 | IEEE binary64; **`float` is an alias** |
| `float128`, `Float128`, `f128` | 128 | Extended precision (often software) |
| `float256`, `Float256`, `f256` | 256 | Research / logical |
| `float512`, `Float512`, `f512` | 512 | Research / logical |

---

## Integer types (complete list)

| Signed (examples) | Unsigned (examples) | Bits |
|-------------------|---------------------|------|
| `int4`, `Int4`, `i4` | `uint4`, `UInt4`, `u4` | 4 |
| `int8`, `i8` | `uint8`, `u8` | 8 |
| `int16`, `i16` | `uint16`, `u16` | 16 |
| `int32`, `i32` | `uint32`, `u32` | 32 |
| **`int`**, `int64`, `i64`, `long` | **`uint`**, `uint64`, `u64`, `usize` | 64 |
| `int128`, `i128` | `uint128`, `u128` | 128 |
| `int256`, `i256` | `uint256`, `u256` | 256 |
| `int512`, `i512` | `uint512`, `u512` | 512 |

---

## Type rules (compiler-enforced)

| Rule | Error example |
|------|----------------|
| Same width required for `+` `-` `*` `/` on scalars | `int32 + int64` |
| No signed/unsigned mix without cast | `int32 + uint32` |
| No `int` + `float` without cast | unchanged |
| No silent widening | `float32` var ← `float64` literal without suffix/cast |

---

## Literal suffixes

Suffix is parsed **after** the numeric token (not part of `stod` / integer digits).

### Float suffixes

| Literal | Inferred type |
|---------|----------------|
| `3.14` | `float64` |
| `3.14f32` | `float32` |
| `1.0f16` | `float16` |
| `0.5f8` | `float8` |

### Integer suffixes

| Literal | Inferred type |
|---------|----------------|
| `42` | `int` (64-bit signed) |
| `42i32` | `int32` |
| `42u` | `uint` (64-bit unsigned) |
| `255u8` | `uint8` |

### Example (typed locals)

```nim
def integrate_step(dt: float32) -> float32
  requires dt > 0.0f32
  ensures result >= 0.0f32
  decreases 0
=
  var x: float32 = 1.0f32
  var y: float32 = 2.0f32
  return x + y
```

**Tests:** `li-tests/typecheck/literal_suffix_ok.li`, `scalar_width_ok.li`, `scalar_width_mix_fail.li`

---

## Binary type (`binary`)

Use **`binary`** for **bit-packed** data (quantized weight masks, sign planes, custom packings).  
Use **`bytes`** for **byte-aligned** buffers (I/O, UTF-8, network).

| | `binary` | `bytes` |
|---|----------|---------|
| Alignment | Bit-oriented semantics | Byte-oriented |
| Literals | `0b1010`, `0b11110000` | string / runtime buffers |
| Arithmetic | Not in v1 (assign/call only) | via std APIs |
| Facade | `std/binary/binary.li` | `std/bytes/` |

### Binary literals

```nim
def weight_mask() -> binary
  requires true
  ensures true
  decreases 0
=
  return 0b10110100

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var mask: binary = weight_mask()
  return 0
```

**Tests:** `li-tests/typecheck/binary_literal_ok.li`

### Roadmap (binary)

- Bitwise ops (`and`, `or`, `xor`, shifts) with proofs
- MIR/codegen for packed storage
- Contract literals referencing `binary` (`ensures result == 0b…`) when proof surface supports them

---

## Choosing accuracy in physics packages

`physics.core` stores **metadata only** — the compiler does not read `float_bits` to change codegen yet. Callers use it to select kernels, assets, or code paths.

### `ScalarPrecision`

| Field | Meaning |
|-------|---------|
| `float_bits` | Preferred float scalar width (4, 8, 16, 32, 64, …) |
| `int_bits` | Preferred int scalar width |
| `weights_encoding` | `0` = float tensor weights; `1` = **binary** packed weights |

### Presets

| Function | `float_bits` | `int_bits` | `weights_encoding` |
|----------|--------------|------------|---------------------|
| `precision_default()` | 64 | 64 | 0 |
| `precision_float32()` | 32 | 32 | 0 |
| `precision_quantized_fp8()` | 8 | 8 | 0 |
| `precision_binary_weights()` | 1 | 1 | 1 |

### `PhysicsProfile`

| Field | Meaning |
|-------|---------|
| `float_bits` | Simulation float width (default 64 in `default_physics_profile`) |
| `int_bits` | Simulation int width |
| `tier`, `dt`, `substeps`, `targets` | Unchanged tier/integrator metadata |

```nim
import physics.core

def configure_arcade_fast() -> PhysicsProfile
  requires true
  ensures result.float_bits == 32
  decreases 0
=
  var p: PhysicsProfile = profile_for_tier(physics_tier_arcade())
  p.float_bits = 32
  p.int_bits = 32
  return p
```

Package README: [packages/li-physics-core/docs/scalar-precision.md](../../packages/li-physics-core/docs/scalar-precision.md)

---

## `li.toml` (project preference, optional)

Documents intent for agents and humans; **not enforced** on dependencies.

```toml
[numerics]
default_float = "float32"   # or float64, float16, float8, …
default_int = "int64"
# default_weights = "binary"   # documentation only until manifest schema lands
```

See [li.toml manifest](li-toml.md).

---

## Codegen maturity

| Feature | Typecheck | MIR / LLVM |
|---------|-----------|------------|
| Width names (`float32`, `int16`, …) | Yes | Partial |
| Width mismatch errors | Yes | N/A |
| Literal suffixes | Yes | Literals often lower as 64-bit today |
| `float32` arithmetic | Yes | Often `double` in LLVM until width-aware lowering |
| `float4` / `float8` packed | Yes | Logical; packed ops TBD |
| `binary` type + `0b` literals | Yes | Stub / no dedicated runtime yet |

Track gaps: [Provability gaps](../verification/provability-gaps.md).

---

## Agent checklist

1. Read this page before adding quantization or physics numerics.
2. Use explicit widths in new APIs (`float32`, not bare `float`) when the module targets narrow storage.
3. Use `binary` + `weights_encoding = 1` for bit-packed weights; do not overload `bytes`.
4. Do not add org-wide CI rules that mandate a single `float_bits`.
5. Run `li-tests/typecheck/scalar_width_*`, `literal_suffix_ok`, `binary_literal_ok` when touching `numeric_types.cpp` or the lexer.

---

## Precision-polymorphic APIs (math / physics / numerics)

To make **all** widths applicable in shared code (not only `float64`):

| Pattern | Syntax | Doc |
|---------|--------|-----|
| Module alias | `type Real = float32` | [Precision polymorphism](precision-polymorphism.md) § A |
| Generic scalar | `def dot[S](a: S, b: S) -> S` | § B |
| Proposed block | `precision float32: …` | § C (desugars to § A) |

**Examples:** `li-tests/generics/precision_real_alias.li` (Pattern A), `precision_generic_fn.li` (Pattern B def)  
**Copy-paste sample:** `docs/language/examples/precision-physics-math.li`

---

## Related

- [Precision polymorphism](precision-polymorphism.md) — math/physics/numerics patterns
- [Types and data](types-and-data.md)
- [Numerics](numerics.md)
- [Contracts and proofs](contracts-and-proofs.md)
- [Release notes: scalar precision](../release-notes/2026-05-19-scalar-precision-types.md)
