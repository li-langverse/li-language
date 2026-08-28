# Parallelism and vectorization (handbook)

Normative: [execution surface spec](../superpowers/specs/2026-05-25-li-execution-surface.md).

| You want | In `.li` | `lic` flags |
| -------- | -------- | ----------- |
| Multi-core | `parallel for` + disjoint | `--cores=8` |
| SIMD inner | `@vectorized(lanes=8)` | none |
| Fast CI | nothing | `lic check --workspace --jobs=8` |

## Example 1 — MD kernel

```nim
parallel for i in 0..<N
  requires disjoint_atom(i, forces)
  decreases N - i
=
  @vectorized(lanes=4)
  for k in 0..<n_neighbors(i)
    accumulate_lj(i, k, positions, forces)
```

```bash
lic build md_step.li -o md_step --cores=8 --threads-per-core=1
```

## Example 2 — Dot product (SIMD only)

```nim
@vectorized(lanes=8)
for i in 0..<N
  ...
```

## Example 3 — Workspace (compile farm)

```bash
lic check --workspace path/to/li.toml --jobs=8 --max-memory=4096
```
