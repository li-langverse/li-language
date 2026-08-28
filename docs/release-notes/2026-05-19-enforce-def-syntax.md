# Enforce `def` for Li procedures

## Summary

User-facing Li functions must be declared with `def`; bare `proc` is rejected except after `extern` for C ABI.

## Agent continuation

1. Read `docs/language/control-flow-and-functions.md` and `docs/language/overview.md` for the canonical declaration shape.
2. Run `./li-tests/run_all.sh encapsulation` and `lic build` on any touched `.li` after edits.
3. Next: migrate external snippets or course material that still use `proc`; keep `extern proc` and callable `proc(...)` types in annotations per design spec.
4. Blocked: none — parser error is `use 'def' for Li procedures; 'proc' is only allowed after 'extern'`.

## Changed

- `compiler/parser/parser.cpp` — reject top-level / `async` / decorated `proc`; require `def`.
- `scripts/migrate-proc-to-def.py` — one-shot source migrator (181 `.li` files).
- `scripts/check-li-def-syntax.sh` — CI/policy grep gate (`rg`); wired in `scripts/ci.sh` and package mirror `ci.yml`.
- `scripts/sync-package-mirror-def-syntax-pr.sh` — sync `packages/<name>/` to org mirrors.
- `li-tests/encapsulation/proc_syntax_rejected.li` + manifest row (`compile_fail`).
- Handbook and examples under `docs/`, `examples/`, `packages/`, `benchmarks/`, `std/`, `li-tests/`.
- Stripped agent/history header comments from tests, package stubs, `std/` facades, and `scripts/templates/package/library/src/lib.li.template` (CWE test labels unchanged).
- All `packages/*/.github/workflows/ci.yml` — run def-syntax check before `lic build`.

## Downstream (package mirrors)

Open PRs on official mirrors (sync from `packages/<name>/`):

| Repo | PR |
|------|-----|
| `li-std-core` | https://github.com/li-langverse/li-std-core/pull/4 |
| `li-std-math` | https://github.com/li-langverse/li-std-math/pull/5 |
| `li-httpd` | https://github.com/li-langverse/li-httpd/pull/4 |
| `li-net` | https://github.com/li-langverse/li-net/pull/4 |
| `li-demo` | https://github.com/li-langverse/li-demo/pull/5 |

Physics/UI packages (`li-std-physics-*`, `li-std-ui`, …) live in the monorepo only until published as separate repos.

## Not changed

- `extern proc` for FFI (still required after `extern`).
- Callable type spelling `proc(a: T) -> U` in type positions (design spec).
- Future `gpu proc` keyword (spec only; not parsed as user `def` today).
- `decorator def` for execution-decorator macros.

## Breaking

| Before | After |
|--------|--------|
| `proc foo() -> int` | **Compile error** — use `def foo() -> int` |
| `async proc worker()` | `async def worker()` |
| `extern def foo()` | **Compile error** — use `extern proc foo()` |

## Security

N/A — syntax-only; no trusted surface change.

## Performance

N/A — no codegen change.

## Downstream

- Regenerate or hand-fix any out-of-tree `.li` that still uses `proc` for definitions.
- Benchmarks ingest (`benchmarks` repo) picks up Li sources from `lic` on sync; no separate action if tracking `lic` main.
