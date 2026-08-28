# Competitive HPC landscape (Li)

One-page map of who we track and why. **Source of truth:** `benchmarks/competitive/registry.toml`.

Li’s pillar order applies: **provability before speed**. Competitive intel informs gaps and bench honesty; it does not justify unproved `parallel for` or unsafe shortcuts.

## Bench columns (harness today)

| Ecosystem | Track | CSV `lang` | What we compare | Honesty |
|-----------|-------|------------|-----------------|---------|
| C++ / Clang OpenMP | bench_tier2 | `cpp` | MD, matmul, SIMD micro, PDE | Native reference |
| Rust | bench_tier2 | `rust` | Same kernels via shared C core | shared_c_kernel |
| Julia | bench_tier2 | `julia` | Tier-2 physics + micro | shared_c_kernel |
| Li | bench_tier2 | `li` | Same fixtures | mixed (pure_li vs shared) |
| OpenMP / races | bench_tier0 | — | Tier-0 correctness, race rejects | CI strict |

## Watch list (manual review)

| Ecosystem | Why track | Li implication |
|-----------|-----------|----------------|
| Chapel | PGAS HPC language | Locale / distribution model → future G-* |
| Kokkos | Performance portability | Team policies, backends → LLVM/OpenMP alignment |
| SYCL / oneAPI | Heterogeneous offload | Watch; no Li GPU story in v1 |
| Zig | Low-level SIMD/comptime | Patterns for codegen, not a bench column yet |
| NumPy / BLAS | ML tier baselines | Tier-3 only; label BLAS explicitly |

## Process

1. **Registry** — add/change ecosystems in `registry.toml`.
2. **Validate** — `./scripts/check-hpc-competitive.sh`.
3. **Bench** — `bench.py --tier 12`; read `benchmarks/results/latest.csv`.
4. **Review** — quarterly manual release notes for `watch` rows; bump `last_reviewed`.
5. **Gaps** — file master-plan **G-*** items when adoption needs a proof path.

Agents: use skill `hpc-competitive-review`. Rule: `li-hpc-competitive.mdc`.
