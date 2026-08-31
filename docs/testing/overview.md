# Tests and quality gates

All conformance tests live in **`li-tests/`**. Nothing is scattered under `compiler/` as one-off files.

!!! note "Tests vs full proof gate"
    Passing `run_all.sh` exercises the **current** compiler gate (parse, policy, typecheck, borrow, codegen). It does **not** yet mean Lean discharged all contracts. See **[Provability gaps](../verification/provability-gaps.md)**.

## E2E-first policy (master plan v2)

PR gates prioritize **end-to-end** confidence over isolated unit suites:

1. Full **`./li-tests/run_all.sh`** (parse → policy → seal → typecheck → build).
2. **Security:** `run_security.sh`, `stdlib_seal`, `cve_patterns`, `decorator_exploits`.
3. **Physics:** `bench.py --tier 0` (strict stability); tier 2 shared-kernel parity is advisory until MSD harness is fixed.
4. **Proof slice:** `vc_emit_contracts.sh`; Lean `lake build` when `lake` is on PATH (`scripts/ci.sh`).

Unit suites (`math_syntax`, `math_linalg`) run in CI but are not the primary ship bar.

## Run everything

```bash
./scripts/build.sh
export LIC=./build/compiler/lic/lic
./li-tests/run_all.sh
```

CI shortcut:

```bash
./scripts/ci.sh
```

## Manifest

Every test is listed in `li-tests/manifest.toml` with an expected **outcome**:

| Outcome | Meaning |
|---------|---------|
| `parse_ok` | Parser accepts |
| `parse_fail` | Parser rejects |
| `compile_ok` | Builds (types + policy) |
| `compile_fail` | Must reject with diagnostic |
| `verify_ok` | Full `lic build` + proof path |
| `verify_fail` | Proof must fail |

## Test suites (complete list)

| Suite | What it guards |
|-------|----------------|
| `lexer_parser` | Tokens, indentation, parse errors |
| `typecheck` | Types, numeric traps, index errors |
| `prove_reject` | `Any`, `sorry`, missing contracts |
| `contracts_verify` | Small proved examples |
| `borrow` | `mut` / `imm`, use-after-move |
| `effects` | `raises IO`, `raises Alloc` |
| `race_shared_memory` | Parallel **exploit** programs must fail |
| `stdlib_seal` | Prelude / `std/` names cannot be shadowed |
| `decorator_exploits` | Decorator hijack / reserved names must fail |
| `decorators` | Valid `@` decorator stacks must build |
| `simd` | Vector types and lane rules |
| `parallel_codegen` | OpenMP lowering smoke |
| `generics` | PEP 695, `Protocol`, `Callable` |
| `collections` | `list`, `dict`, `tuple`, `enum`, … |
| `runtime` | `extern`, linking |
| `benchmarks` | Tier-0 physics/math correctness |
| `tetris` | Game OOB must fail compile |
| `integration` | Bootstrap compiler build |

## Race exploit suite (security of parallelism)

Files in `li-tests/race_shared_memory/` are **deliberate bugs**:

| File | Attack |
|------|--------|
| `shared_mut_write.li` | All threads write same cell |
| `overlap_par_slice.li` | Overlapping slices |
| `missing_disjoint_clause.li` | No disjoint proof |
| `mut_capture_no_sync.li` | Unsafe capture |
| `borrow_mut_across_iters.li` | `borrow mut` in parallel loop |
| `false_disjoint_proof.li` | Lying proof |

Positive control: `good_disjoint_parallel.li` **must build**.

## Decorator exploit suite (7d-e)

`li-tests/decorator_exploits/` — must **not** compile (policy gate):

| File | Attack |
|------|--------|
| `reserved_def_parallel.li` | `decorator def parallel` (legacy definition form) |
| `typosquat_paralell.li` | `@my_paralell` — decorator name segment `paralell` near `parallel` (W0403 warning, still compiles) |
| `single_segment_name.li` | `decorator def tiled` (one segment) |
| `missing_disjoint_at_parallel.li` | `@parallel(threads=auto)` without `disjoint=` |

Positive: `decorators/cpu_only_ok.li`, `decorators/parallel_with_disjoint.li`.

Run alone:

```bash
./li-tests/run_all.sh decorator_exploits decorators
```

`run_all.sh` accepts one or more suite names; `--ci` may be included with them.

```bash
./li-tests/run_all.sh decorator_exploits decorators --ci
```

```bash
./scripts/test_race_reject.sh
```

## Security testing (malformed input)

Separate from manifest — **`li-tests/run_security.sh`**:

- Every file under `li-tests/security/` (including `proof_safety/`)
- `lic parse` and `lic check` must exit 0 or 1, **never crash** (no signal)
- Generated huge-comment stress

Runs on **every** `scripts/ci.sh` and in the Windows CI job.

Details: [Security audits](security.md).

## Fuzz testing (parser)

**libFuzzer** target: `compiler/fuzz/parse_fuzz`

- Corpus: `compiler/fuzz/corpus/`
- Merge script: `scripts/merge_fuzz_corpus.sh`
- CI: daily [`.github/workflows/fuzz.yml`](https://github.com/li-langverse/li-language/blob/dev/.github/workflows/fuzz.yml) + PR smoke

## Memory / sanitizer (optional CI)

[`.github/workflows/memory.yml`](https://github.com/li-langverse/li-language/blob/dev/.github/workflows/memory.yml):

- Valgrind / ASan smoke on Linux
- Memory profiling scripts under `scripts/profile-memory.sh`

## Benchmark verification

`benchmarks/harness/verify.py` and `stability.py` check physics invariants (energy drift, momentum, …) for C++/Rust/Julia/Li kernels.

## Adding a test

1. Add `something.li` under the right suite folder.
2. Add `[[tests]]` block to `manifest.toml`.
3. For `compile_fail`, set `expected_substr` or add `something.exp`.
4. Run `./li-tests/run_all.sh <suite>`.

See also `li-tests/README.md` in the repository.
