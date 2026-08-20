# Examples gallery

Copy these into a `.li` file and run `lic build file.li -o out`.

## Hello

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  print("Hello from Li")
  return 0
```

## Float arithmetic

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var x: float = 1.5
  var y: float = 2.25
  var z: float = x + y
  return 0
```

## Fixed-size array

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var data: array[4, int]
  var i: int = 0
  while i < 4
    data[i] = i
    i = i + 1
  return 0
```

## Math vs intrinsics

**Preferred (Tier 1 benches, new code):** math on fixed arrays — no `__li_simd_*`.

```li
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
  var s: float = dot(x, y)
  return 0
```

Full walkthrough: [Math-first HPC examples](math-hpc-examples.md).

**Legacy / audit only:** manual SIMD intrinsics (do not use in new benchmarks):

```li
var v: simd[f64, 4] = __li_simd_splat_f64(x)
var p: simd[f64, 4] = __li_simd_mul_f64(v, v)
acc = acc + __li_horiz_sum_f64(p)
```

See [Fast math — intrinsics appendix](fast-math-and-parallelism.md#compiler-intrinsics-appendix).

## Parallel zeroing

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var buf: array[8, float]
  var i: int = 0
  while i < 8
    buf[i] = 1.0
    i = i + 1
  parallel for j in 0..<8
    requires disjoint_elem(j, buf)
    decreases 8 - j
  =
    buf[j] = 0.0
  return 0
```

## Repository paths

| Example | Path |
|---------|------|
| Hello | `examples/hello.li` |
| Arrays | `examples/arrays.li` |
| Tetris (game) | `examples/tetris/main.li` |
| Math dot bench | `benchmarks/tier1_micro/simd_dot/li/main.li` |
| Math matmul bench | `benchmarks/tier1_micro/matmul_naive/li/main.li` |
| MD + parallel | `benchmarks/tier2_physics/md_lennard_jones/li/main.li` |
| Math HPC guide | `docs/guide/math-hpc-examples.md` |
