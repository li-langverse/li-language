# Precision-polymorphic math and physics

How to write **one API** that applies at **every** scalar width (`float32`, `float16`, `float8`, …) without the org forcing a single accuracy.

**Prerequisites:** [Scalar precision](scalar-precision.md) (type names, suffixes, `binary`, physics metadata).

**Who picks the width?** **You do** — via `type Real = float32`, generics `[S]`, literal suffixes, `li.toml` notes, or `PhysicsProfile.float_bits`. Li does not pick a single org-wide accuracy; see [You set precision yourself](scalar-precision.md#you-set-precision-yourself).

---

## Goal

| Requirement | Approach |
|-------------|----------|
| Same formula at f32 and f64 | Generic `def` with one type parameter `[S]` |
| Module ships as FP32 build | `type Real = float32` at top of file |
| Simulation picks width at runtime | `PhysicsProfile.float_bits` + dispatch (today) |
| Bit-packed weights | `binary` + `precision_binary_weights()` |

The compiler **always** checks that widths are not mixed silently. Polymorphism means **the same type variable `S` everywhere in a call**, not automatic conversion.

---

## Pattern A — `type Real = …` (module sugar, **today**)

Pick one width for the whole file. This is the simplest “syntactic sugar” that already works.

```nim
# packages/li-physics-relativity — hypothetical FP32 build
type Real = float32

type Vec3r = object
  public x: Real
  public y: Real
  public z: Real

def lorentz_gamma(v: Real, c: Real) raises IO -> Real
  requires c > 0.0f32
  requires v >= 0.0f32
  ensures result >= 1.0f32
  decreases 0
=
  var beta: Real = v / c
  if beta >= 1.0f32:
    return 1.0f32
  return 1.0f32 / li_rt_sqrt(1.0f32 - beta * beta)

def time_dilation_proper(dt: Real, gamma: Real) -> Real
  requires gamma >= 1.0f32
  ensures result == dt / gamma
  decreases 0
=
  return dt / gamma
```

Duplicate the module with `type Real = float64` for an FP64 variant, **or** use Pattern B once per function.

**Literal rule:** use suffixes that match `Real` (`1.0f32` when `Real = float32`).

---

## Pattern B — generics `[S]` (function sugar, **today**)

One function, many widths — **if every use of `S` is consistent** and contracts only relate parameters (not mixed literals).

```nim
def dot3[S](ax: S, ay: S, az: S, bx: S, by: S, bz: S) -> S
  requires true
  ensures result == ax * bx + ay * by + az * bz
  decreases 0
=
  return ax * bx + ay * by + az * bz

def scale[S](x: S, k: S) -> S
  requires true
  ensures result == x * k
  decreases 0
=
  return x * k
```

**Intended call sites (when monomorph is complete):**

```nim
var a32: float32 = dot3(1.0f32, 0.0f32, 0.0f32, 2.0f32, 1.0f32, 0.0f32)
var a64: float64 = dot3(1.0, 0.0, 0.0, 2.0, 1.0, 0.0)
```

Until then, instantiate Pattern B by **duplicating** one `type Real = …` module per width (Pattern A).

**Arrays at one precision (generic alias — experimental):**

```nim
type Block[S] = array[4, S]
```

Prefer `array[64, Real]` with `type Real = float32` for production packages today.

### Compiler maturity (honest)

| Feature | Status |
|---------|--------|
| Pattern A `type Real = float32` | Typecheck + build ✓ — `li-tests/generics/precision_real_alias.li` |
| Pattern B `def f[S](x: S) -> S` | Definition ✓ — `precision_generic_fn.li`, `identity.li` |
| Pattern B calls `f(1.0f32, …)` from `main` | Call-site monomorph — **in progress** |
| `ensures` with `*` on generic `S` | Prefer `ensures true` or Pattern A until contract typing catches up |
| `return 1.0f32` from `-> float32` | Prefer `return x` (variable) until LLVM width lowering is complete |

### Contract pitfalls

| Bad | Why |
|-----|-----|
| `requires c > 0.0` inside `def f[S](c: S)` | `0.0` is `float64`; `S = float32` → width error |
| `ensures result >= 1.0` | Same — use `1.0f32` only in f32 monomorph, or prove from parameters |
| `ensures result >= 1.0f32` when `S` may be `float64` | Too strong / wrong width |

Prefer **`ensures result == <expr in S>`** using only parameters and suffix literals at the **call site**, not width-specific literals in the generic body’s contracts.

---

## Pattern C — `precision` block (**proposed** syntax)

Not in the compiler yet; target desugaring is Pattern A.

```nim
precision float32:
  type Real = float32
  type Block = array[64, Real]

  def euler_step(y: Block, dt: Real, fx: Real) -> unit
    requires dt > 0.0f32
    ensures true
    decreases 0
    =
    var i: int = 0
    while i < 64
      y[i] = y[i] + fx * dt
      i = i + 1
```

Equivalent today: put the same lines in a file without the `precision` wrapper.

Future: `precision float8:` / `precision binary:` blocks for quantization kernels.

---

## Pattern D — physics + numerics wiring

1. **Declare** width in `physics.core`:

```nim
import physics.core

def profile_fp32() -> PhysicsProfile
  requires true
  ensures result.float_bits == 32
  decreases 0
=
  var p: PhysicsProfile = profile_for_tier(physics_tier_simulation())
  p.float_bits = 32
  p.int_bits = 32
  return p
```

2. **Implement** math at that width (Pattern A or B in `li-math`, `li-math-numerics`, `li-physics-*`).

3. **Dispatch** (until the compiler reads `float_bits`):

```nim
def integrate_step(profile: PhysicsProfile, dt: float) -> float
  requires profile.float_bits == 32
  ensures true
  decreases 0
=
  # call float32-specialized kernel when profile.float_bits == 32
  return dt
```

4. **Binary weights:** `precision_binary_weights()` + `binary` buffers for masks; float kernels stay in `Real`.

---

## Package checklist

| Package | Should |
|---------|--------|
| `li-math` | Export width-agnostic `def` with `[S]` where possible; document default `type Real = float64` in README |
| `li-math-numerics` | `array[N, S]` integrators generic in `S`; FP64 reference + FP32 fast path files |
| `li-physics-*` | Use `Real` alias or `[S]`; set `PhysicsProfile.float_bits` in examples |
| Agents | Never assume `float` in imported API — read module’s `Real` or suffixs at call site |

---

## Quick comparison

| Pattern | Syntax | All precisions? | Best for |
|---------|--------|-----------------|----------|
| A `type Real = float32` | Today | One per file | Package release (fp32 build) |
| B `def f[S](x: S) -> S` | Today | Yes, per call | Shared formulas in `li-math` |
| C `precision float32:` | Proposed | One per block | Ergonomics / less boilerplate |
| D `PhysicsProfile.float_bits` | Today (metadata) | Runtime choice | Games, A/B accuracy |

---

## Related

- [Scalar precision](scalar-precision.md)
- [Collections and generics](collections-generics.md)
- [packages/li-physics-core/docs/scalar-precision.md](../../packages/li-physics-core/docs/scalar-precision.md)
