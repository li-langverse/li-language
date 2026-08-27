# Types and data

Li’s type system follows **Python 3.14 typing** habits, but programs compile to **fixed-size machine types**. There is no `Any`.

## Scalar types (everyday names)

| Li name | Machine type | Notes |
|---------|--------------|-------|
| `int` | `i64` | Default integer; not unlimited precision |
| `uint` | `u64` | Unsigned; no silent mix with `int` |
| `float` | `float64` | IEEE binary64 |
| `bool` | `bool` | Not a number |
| `str` | string | UTF-8 at runtime |
| `unit` | void-like | “No useful value” |
| `binary` | bit-packed | Quantized masks / weights (`0b…` literals); not `bytes` |

## Fixed-width integers and floats

Full tables (including `float4` … `float512`, `int4` … `int512`): **[Scalar precision](scalar-precision.md)**.

| Signed | Unsigned | Float (examples) |
|--------|----------|-------------------|
| `i8` … `i128`, `int32` | `u8` … `u128`, `uint32` | `float16`, `float32`, `float64` (`float`) |
| `int4` … `int512` | `uint4` … `uint512` | `float4`, `float8`, … `float512` |

Mixing widths without a cast is an error. The ecosystem does **not** enforce one global accuracy — projects and physics profiles choose per module.

## Complex numbers

| Type | Layout |
|------|--------|
| `complex` / `complex128` | Two `float64`: real + imaginary |
| `complex64` | Two `float32` |

## SIMD vectors

```nim
var v: simd[f64, 4]
var w: simd[f32, 8]
```

Packed lanes for HPC. See [SIMD and parallel](simd-parallel.md).

## Arrays (fixed size)

```nim
var grid: array[64, float]
var ids: array[128, int]
```

- Size `N` is part of the type.
- Indexing must stay in bounds (proved or checked).

## Collections (heap)

| Type | Python analogue |
|------|-----------------|
| `list[T]` | `list` |
| `dict[K, V]` | `dict` |
| `tuple[...]` | `tuple` |
| `TypedDict` | `TypedDict` |
| `frozenset[T]` | immutable set view |

Allocation may carry `raises Alloc`.

## Named shapes

Type and object names use **PascalCase** (`ClassName`) — see [Naming conventions](naming-conventions.md). Fields use **snake_case**.

```nim
type Point = object
  x: float
  y: float

type Color = enum
  Red, Green, Blue
```

## Refinement types (value domains)

```li
type NonNeg = {x: int | x >= 0}
type Index = {i: int | 0 <= i and i < N}
```

A refinement declares **which values** a name may take. Parameters and `var` bindings of that type are checked at **calls** and **initializers**:

- Provably **outside** the predicate → **E0305** (compile error).
- **Inside** but not yet provable → proof obligation (see [Refinement types](refinement-types.md)).

Index refinements (`Index`, `Index10`, …) use the same syntax for array safety; see `li-tests/contracts_verify/index_refinement.li`.

## Callable and Protocol

```nim
type Handler = Callable[[int], bool]
type Sized = Protocol["__len__", int]
```

Generics use PEP 695 style:

```nim
def identity[T](x: T) -> T
```

## What is forbidden

| Forbidden | Why |
|-----------|-----|
| `Any` | No static guarantee |
| `unsafe` | Bypasses proof |
| Bare `cast[T](e)` | Need proof-carrying cast |
| `sorry` / `assume` | Fake proofs |

## Cast with proof

```nim
cast[T](value, proof)
```

Only when a proof term shows the cast is valid.

More numbers detail: [Numerics](numerics.md).
