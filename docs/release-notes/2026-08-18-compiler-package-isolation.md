# Compiler ↔ package isolation (carve-out)

**Date:** 2026-08-18

## Summary

The compiler is now verified **upstream-only**: `compiler/`, `runtime/`,
`bootstrap/`, and the `std/` facades never depend on, embed, or hardcode the
downstream package tree (`packages/`). Packages are user code compiled *by*
`lic`, never linked *into* it — and `li-aimd` is verified to install and run
standalone outside the monorepo.

## Changed

- **`scripts/check-compiler-isolation.sh` (new):** fails if
  - `compiler/`, `runtime/`, or `bootstrap/` source references `packages/`
    paths (documentation comments exempt);
  - compiler or runtime CMake references the package tree;
  - `std/` facades import a non-std module;
  - any package is nested (every package is top-level `packages/<name>/li.toml`);
  - any `[dependencies]` path escapes `packages/` (must be `path = "../<name>"`).
- **`scripts/check-aimd-standalone.sh` (new), generalized to
  `scripts/check-package-standalone.sh`:** for each workspace member, copies the
  package plus its transitive `[dependencies]` closure into a fresh temp tree,
  builds a program that imports it, and runs it — proving each package is
  installable by itself, not coupled to the monorepo. `check-aimd-standalone.sh`
  remains as the single-package form; CI runs the generalized gate on the core
  scientific packages, and `bash scripts/check-package-standalone.sh` runs the
  full 36-member sweep.
- **`scripts/ci.sh`:** gates run as CI phases (`compiler isolation (carve-out)`,
  `per-package standalone install (carve-out)`).
- **`docs/architecture/overview.md`:** new "Compiler ↔ package isolation
  (carve-out)" section documenting the boundary and the gates.

## Verification

```bash
./scripts/check-compiler-isolation.sh   # ok (compiler upstream-only)
./scripts/check-package-standalone.sh li-aimd li-chem li-sim li-nanoreactor
# full sweep: bash scripts/check-package-standalone.sh
```

Negative tests: an escaping dependency (`path = "/etc/evil"`) and a missing
dependency (`../li-chem-missing`) both fail the gates as expected.

## Standalone gaps surfaced by the sweep — now closed

All **38 workspace members** pass the per-package standalone gate (build + run
outside the monorepo). Fixes landed in this change:

| Package | Fix |
|---------|-----|
| `li-math` | codegen: `create_user_call` strips fast-math flags for aggregate returns |
| `li-math-numerics` | mutating integrators now take `var array` params (E0311) |
| `li-physics-weather/em/fluids` | E0201 dynamic-index gaps: weather/em unrolled to constant-index steps or `var array` params; fluids routes `cloth_pbd_distance` indices through a bounds-ensuring helper |
| `li-render` | `RenderFpsCounter.frame_count` is `float` (no int→float cast exists in Li); HUD text procs declare `raises Alloc` + `ensures result != ""`; codegen `ReturnVoid` now returns `null` for pointer-returning procs (`ret i0 0` verifier error) |
| `li-studio` | `lig.present` package added; gui/object-arg call sites now flatten call-returned and field-access object args (was silently dropping them); dropped an out-of-scope `requires scene_entity_count >= 0` |
| `li-net-httpd` | value-returning procs got honest `ensures` bounds instead of `ensures true` (E0303); macOS `#else` stub added for `epoll_wait_tagged_timeout_ms_i` (was missing → link failure); new `li-tests/tooling/check_httpd_epoll_seam_runtime.sh` gate builds + runs the seam on every platform |
| `li-gui`, `li-scene` | codegen: object-return calls with nested object args now flatten correctly |
