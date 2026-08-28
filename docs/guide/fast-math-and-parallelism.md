# Fast math and parallelism

!!! note "Status"
    **Math surface** (`dot`, `a @ b`, 2d `C = A @ B`, `sum`, element-wise ops) is **implemented** on fixed `array` tiles. **`@vectorized(lanes=4)`** on `def` is validated; **`@no_vectorize`** disables f64×4 array SIMD in codegen. **`@vectorized` on `for`** parses but does not elaborate loops yet. **`@parallel`** requires `disjoint=`. See **[Provability gaps](../verification/provability-gaps.md)**.

Li is built for **scientific and high-performance** work. Prefer:

1. **Math on arrays** — `dot(x, y)`, `x @ y`, `C = A @ B`, `sum(a)`, scalar loops for AXPY.
2. **`parallel for`** — many CPU cores with proof that iterations do not alias shared memory.

You do **not** install NumPy, OpenMP bindings, or a thread library yourself — Li’s compiler wires native vector instructions and (on Linux) OpenMP when your program passes the proof gate.

**Start here:** [Math-first HPC examples](math-hpc-examples.md) · [Linear algebra](../language/linear-algebra.md)

---

## Math surface (preferred)

| You write | Meaning |
|-----------|---------|
| `dot(x, y)` or `x @ y` | Inner product of matching `array[N, float]` |
| `C = A @ B` | 2d matmul on `array[M, array[K, float]]` tiles |
| `sum(a)` | Reduction over fixed `array[N, int]` or `array[N, float]` |
| `y[i] = alpha * x[i] + y[i]` | AXPY via index loop (element-wise arrays planned) |

Tier 1 benchmarks use this style — see `benchmarks/tier1_micro/simd_dot/li/main.li` and `matmul_naive/li/main.li` (no `__li_simd_*` in user sources).

### Closed dot certificate

The fixed-size float dot is a finished certificate: `li-tests/contracts_verify/linalg_dot4_float_closed.li` proves all three forms with no open proof obligations —

```li
def dot4_float(x: array[4, float], y: array[4, float]) -> float
  requires true
  ensures result == x[0] * y[0] + x[1] * y[1] + x[2] * y[2] + x[3] * y[3]
  decreases 0
=
  return x @ y
```

1. **Closed form** — `return x @ y` is witnessed equal to the expanded sum.
2. **Handwritten loop** — `acc = acc + x[i] * y[i]` with `while i < 4` is witnessed equal to the same sum (any parameter/accumulator/index names).
3. **Concrete value** — filling `a` with `1.0` and `b` with `2.0`, then `s = dot4_float(a, b)`, folds to `s == 8.0`: the counter loop is unrolled so `a[i] = 1.0` stores are tracked per element, and the callee `ensures` is constant-folded at the call site.

---

## Parallel loops

```li
parallel for j in 0..<8
  requires disjoint_elem(j, buf)
  decreases 8 - j
=
  buf[j] = 0.0
```

| Piece | Meaning |
|-------|---------|
| `parallel for` | Run iterations on multiple cores |
| `0..<8` | `j` runs 0, 1, …, 7 |
| `requires disjoint_elem(...)` | **Proof obligation**: each iteration touches different memory |
| Body | What each iteration does |

If Li cannot see that iterations are independent, **`lic build` fails**. That is how Li prevents data races by default.

### Thread count

```bash
lic build sim.li -o sim --threads=8
```

Or set `LI_OMP_THREADS=8` in the environment before running the binary (Linux with OpenMP linked).

---

## Putting it together

A typical HPC workflow:

1. Store fields in `array[N, float]` or 2d `array[M, array[K, float]]`.
2. Express kernels with `dot` / `@` / `sum` or proved `parallel for`.
3. Let the compiler lower to scalar/SIMD/OpenMP (see appendix for MIR names).

See `benchmarks/tier2_physics/md_lennard_jones/li/main.li` for a pure-Li MD driver.

---

## Compiler intrinsics appendix

Low-level SIMD types remain for **compiler tests and codegen debugging** — not for new handbook or benchmark sources.

```li
var v: simd[f64, 4] = __li_simd_splat_f64(1.5)
```

| Call | Effect |
|------|--------|
| `__li_simd_splat_f64(x)` | Broadcast scalar to all lanes |
| `__li_simd_mul_f64(a, b)` | Lane-wise multiply |
| `__li_simd_add_f64(a, b)` | Lane-wise add |
| `__li_horiz_sum_f64(v)` | Add all lanes into one `float` |

Lane counts **4 and 8** are supported; other sizes are rejected at compile time.

**Migration:** replace manual intrinsic loops with `dot` / `@` on `array` tiles; see [Examples gallery](examples-gallery.md#math-vs-intrinsics).

More detail: [SIMD & parallel reference](../language/simd-parallel.md).

---

## What is not allowed (on purpose)

| You cannot… | Why |
|-------------|-----|
| Share one variable across parallel iterations without proof | Data races |
| Mix `int` and `float` silently | Catches science bugs |
| Skip `requires` / `ensures` / `decreases` | No proof without promises |
| Use `Any` or `unsafe` | Breaks the guarantee story |
