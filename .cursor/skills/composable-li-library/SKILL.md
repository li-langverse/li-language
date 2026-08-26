---
name: composable-li-library
description: >-
  Checklist for Li packages and std slices: export composable lib.li API with
  lifecycle verbs, thin main, li-tests that import lib. Use before merging
  packages/**, std/**, or when adding httpd, bench runners, or lip tools.
---

# Composable Li library

**Canonical:** [composable-by-default.md](../../../docs/ecosystem/composable-by-default.md)

## Before merge

1. **`src/lib.li` exists** and exports the feature as small **`def`** APIs (not only `main`).
2. **Lifecycle documented** in README: start, stop, ready (or domain equivalents).
3. **`src/main.li`** (if present) only demos — imports `lib`, no business logic duplication.
4. **Contracts** on every new/changed **`def`** and each **`extern proc`** (`requires` / `ensures` / `decreases`).
5. **`li-tests` imports lib** — add or extend `li-tests/composable/` (or package `li-tests/`) with `verify_ok` / `compile_ok` that `import`s the module; do not rely on exec-only smoke.
6. **No false ship claims** — stubs are fine; say “aspirational API” if P0 gates block real I/O.

## Review questions

- Can another package `import` this module and call `serve` / `stop` from its own `def main` without copy-paste?
- Is there an explicit handle (or config + result type) instead of a hidden global?
- Would an agent find spawn/teardown from README + `lib.li` in one pass?

## Commands

```bash
./scripts/build.sh
export LIC=./build/compiler/lic/lic
./li-tests/run_all.sh composable
lic build packages/<name>/src/lib.li -o /dev/null
```

## Related

- `create-li-package` — scaffold with `lib.li` first
- `strict-by-default-gate` — composable APIs still need proof contracts
- `.cursor/rules/li-composable.mdc`
