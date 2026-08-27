# SIMD and parallel execution

Li targets **CPU HPC**: vector lanes on one core, many cores on shared memory — **without** user-installed parallel frameworks.

!!! note "Provability status"
    Disjointness for `parallel for` is enforced today partly via **string heuristics** in `policy.cpp`, not full Lean discharge. Decorators (`@parallel`, …) **parse** but do not yet elaborate. See **[Provability gaps](../verification/provability-gaps.md)** (**G-par**, **G-dec**).

## Two layers

| Layer | Syntax | Hardware |
|-------|--------|----------|
| SIMD | `simd[T, N]` | AVX / NEON vector units |
| Multi-core | `parallel for` | `li_parallel_for` (`--cores`) (linked by `lic build`) |

Inner SIMD + outer `parallel for` is the standard Li pattern for hot loops.

## SIMD type

```nim
var a: simd[f64, 4]
var b: simd[f32, 8]
```

Rules (v1):

- `T` ∈ integer or float lane types (`f32`, `f64`, `i32`, …).
- `N` ∈ `{4, 8}` in the current compiler (spec allows more).
- No silent fallback to scalar if the CPU cannot do `N` lanes.

### Intrinsics (today)

| Intrinsic | Role |
|-----------|------|
| `__li_simd_splat_f64(x)` | Broadcast scalar |
| `__li_simd_mul_f64(a, b)` | Lane-wise multiply |
| `__li_simd_add_f64(a, b)` | Lane-wise add |
| `__li_horiz_sum_f64(v)` | Sum lanes to scalar |

Stdlib names like `horizontal_sum` and `dot` are planned to wrap these.

## `parallel for`

```nim
parallel for i in 0..<N
  requires disjoint_row(i, grid)
  invariant row_ok(i, grid)
  decreases N - i
=
  compute_row(grid, i)
```

### Proof names the checker knows

Include a disjointness clause in source, for example:

- `disjoint_row(i, grid)`
- `disjoint_elem(i, buf)`
- `disjoint_slice(...)`

Without such a clause, **`lic build` rejects** the loop.

### What is rejected (by design)

| Pattern | Result |
|---------|--------|
| All threads write `buf[0]` | Compile error |
| Overlapping `par_slice` ranges | Compile error |
| Mutable capture without `Sync` | Compile error |
| `borrow mut` across iterations | Compile error |
| Fake disjoint proof | Lean / VC rejection |

Exploit fixtures live in `li-tests/race_shared_memory/`.

## Thread count

```bash
lic build app.li -o app --cores=8 --threads-per-core=1
./app
```

See [execution surface](../superpowers/specs/2026-05-25-li-execution-surface.md). Native pool replaces OpenMP on user link path.

## `Send` and `Sync` (spec)

Values captured into parallel regions must be safe to share. Laws are defined in the Lean semantics; the surface syntax enforces `Sync` when shared mutable state appears.

## GPU (future)

`gpu proc` and device buffers are **not** v1 — CPU SIMD + OpenMP first.

Benchmarks: [Benchmarks guide](../benchmarks.md).
