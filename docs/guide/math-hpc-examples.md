# Math-first HPC examples

Write numerical kernels as **ordinary math** on fixed-size `array` tiles. The compiler lowers them to loops, SIMD, and (when proved) OpenMP — you do **not** call `__li_simd_*` in application or Tier 1 benchmark code.

| Surface (you write) | Status in `lic` today | Tier 1 bench |
|---------------------|----------------------|--------------|
| `dot(a, b)` / `a @ b` on `array[N, float]` | Implemented | [`simd_dot`](../../benchmarks/tier1_micro/simd_dot/li/main.li) |
| `C = A @ B` on `array[M, array[K, float]]` | Implemented (fixed shapes) | [`matmul_naive`](../../benchmarks/tier1_micro/matmul_naive/li/main.li) |
| Blocked IKJ `C[i][j] += …` on tiles | Implemented (manual loops) | [`matmul_blocked`](../../benchmarks/tier1_micro/matmul_blocked/li/main.li) |
| `sum(a)` on `array[N, int\|float]` | Implemented | — |
| Element-wise `a * b` on arrays | Implemented (**2i-a**) | — |
| `@vectorized(lanes=4)` / `@no_vectorize` on `def` | Policy + array SIMD gate (**7d-b**) | — |
| `@vectorized` on `for` | Parse only | — |
| `@parallel` on `def` / loops | Policy + OpenMP when proved | — |

See also: [Linear algebra](../language/linear-algebra.md), [Fast math & parallelism](fast-math-and-parallelism.md), [math/linalg plan](../superpowers/plans/2026-05-16-li-math-linalg-surface.md).

---

## Dot product (1d)

`dot(x, y)` and `x @ y` are the same when `x` and `y` are both `array[N, float]` with the same `N`.

```li
def dot3(x: array[4, float], y: array[4, float]) -> float
  requires true
  ensures result >= 0.0
  decreases 0
=
  return x @ y

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var x: array[4, float]
  var y: array[4, float]
  var i: int = 0
  while i < 4
    x[i] = 1.0
    y[i] = 2.0
    i = i + 1
  var s: float = dot3(x, y)
  return 0
```

**Shape error (compile fail):** mismatched lengths — `li-tests/math_linalg/array_dot_mismatch.li`.

**Large workloads:** Tier 1 `simd_dot` uses repeated `array[40000, float]` tiles (see benchmark `main.li`); no intrinsics in the user file.

---

## AXPY (scalar loop form)

Until element-wise `alpha * x + y` on whole arrays lands, express AXPY with an index loop (still math — no `simd(...)`):

```li
def axpy(alpha: float, x: array[8, float], y: array[8, float]) -> unit
  requires true
  ensures true
  decreases 0
=
  var i: int = 0
  while i < 8
    y[i] = alpha * x[i] + y[i]
    i = i + 1

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var x: array[8, float]
  var y: array[8, float]
  var i: int = 0
  while i < 8
    x[i] = 1.0
    y[i] = 0.0
    i = i + 1
  axpy(2.0, x, y)
  return 0
```

**Target form (Phase 2i/7e):** same semantics with `@vectorized(lanes=8)` on the loop; decorators elaborate when **7d** lowering is complete.

---

## Matrix multiply (2d `@`)

Fixed 2d tiles use nested arrays: `array[M, array[K, float]] @ array[K, array[N, float]]` → `array[M, array[N, float]]`.

```li
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var A: array[2, array[3, float]]
  var B: array[3, array[2, float]]
  var C: array[2, array[2, float]]
  A[0][0] = 1.0
  B[0][0] = 1.0
  C = A @ B
  return 0
```

**Inner dimension mismatch** fails at compile time — `li-tests/math_linalg/matmul_dim_mismatch.li`.

**Benchmark note:** `matmul_naive` and `matmul_blocked` use many **64×64** tiles so stack size stays bounded; blocked variant uses **BK=16** IKJ micro-blocks (`matmul_blocked_tile_f64`, 512 reps ≈ 512³ flops).

---

## Reduction: `sum`

```li
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var a: array[4, int]
  var i: int = 0
  while i < 4
    a[i] = i + 1
    i = i + 1
  var s: int = sum(a)
  return 0
```

---

## MD-style force loop (scalar math today)

Molecular dynamics kernels are still written as **scalar** loops over particles; Tier 2 `md_lennard_jones` is pure Li without manual SIMD intrinsics in the driver. Parallel safety uses `parallel for` + `disjoint_elem` (see [Fast math & parallelism](fast-math-and-parallelism.md)).

```li
# Sketch — full source: benchmarks/tier2_physics/md_lennard_jones/li/main.li
parallel for i in 0..<N
  requires disjoint_elem(i, forces)
  decreases N - i
=
  forces[i] = compute_pair_force(i)
```

---

## Math source vs compiler expansion (audit)

| You ship (benchmarks, handbooks) | Compiler may emit (MIR / LLVM appendix) |
|----------------------------------|----------------------------------------|
| `return a @ b` | `ArrayDotF64` loop (SIMD autovec at `-O3`) |
| `C = A @ B` (2d) | `ArrayMatMul2DF64` triple loop |
| `sum(a)` | `ArraySumF64` / `ArraySumI64` |

**Do not** paste `__li_simd_splat_f64` / `__li_simd_mul_f64` into new examples — use [Compiler intrinsics appendix](fast-math-and-parallelism.md#compiler-intrinsics-appendix) only when debugging codegen.

---

## Run and test

```bash
./scripts/build.sh
./build/compiler/lic/lic build my_kernel.li -o my_kernel
./li-tests/run_all.sh   # math_linalg suite
python3 benchmarks/harness/bench.py --tier 1   # after build
```

Regression tests: `li-tests/math_linalg/`.
