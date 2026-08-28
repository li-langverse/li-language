# li-tests

Central **test repository** for Li. All conformance, rejection, verification, and benchmark-correctness tests live here — not scattered under `compiler/` or ad-hoc folders.

## Run everything

```bash
export LIC=/path/to/lic   # optional; default ../build/lic
./li-tests/run_all.sh
```

Single suite:

```bash
./li-tests/run_all.sh race_shared_memory
./li-tests/run_all.sh typecheck
./li-tests/run_all.sh prove_reject
```

## Layout

| Suite | Path | What it checks |
|-------|------|----------------|
| `lexer_parser` | `lexer_parser/` | Tokenize + parse; AST snapshots |
| `typecheck` | `typecheck/` | Python 3.14 types, refinements, science traps |
| `prove_reject` | `prove_reject/` | Must fail: `Any`, `sorry`, missing contracts |
| `contracts_verify` | `contracts_verify/` | Must pass Lean on small proved examples |
| `borrow` | `borrow/` | Use-after-move, double `mut` |
| `race_shared_memory` | `race_shared_memory/` | **Exploit races** — must fail compile |
| `parity_python` | `parity_python/` | `.li` vs `.pyi` / mypy baseline |
| `mir_llvm` | `mir_llvm/` | MIR / LLVM golden output |
| `benchmarks` | `benchmarks/` | Tier 0 correctness + invariants |
| `integration` | `integration/` | Tetris, hello, end-to-end build |

## Manifest

`manifest.toml` lists every fixture with expected outcome:

- `compile_ok` — `lic build` succeeds  
- `compile_fail` — must reject (diagnostic substring in `*.exp`)  
- `verify_ok` — `lic build` + Lean discharge  
- `parse_ok` / `parse_fail` — parser-only stages  

## CI

```bash
./li-tests/run_all.sh --ci
```

Fails if any manifest entry mismatches expected outcome.

## Adding a test

1. Place `.li` in the right suite directory  
2. Add entry to `manifest.toml`  
3. For `compile_fail`, add `filename.exp` with required diagnostic substring  
4. Run `./li-tests/run_all.sh <suite>`

## Related docs

- [Language design spec](../docs/superpowers/specs/2026-05-14-li-language-design.md)  
- [Benchmarks plan](../docs/superpowers/plans/2026-05-14-benchmarks-and-simulations.md)  
