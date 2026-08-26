# `li.toml` manifest

**Schema authority:** [lip plan § A3](../superpowers/plans/2026-05-16-li-package-manager-lip.md) (package metadata, dependencies, workspace).

## Strict by default

Gates are **on at maximum** unless this file **explicitly** lowers them. Omission does not disable proof, security, or performance checks. See [Strict by default](../ecosystem/strict-by-default.md).

There is **no optional provability** by default.

### Explicit downgrade — `[gates]` (stub)

Parser support may lag; the **policy** is fixed: any relaxation must be visible in the manifest.

```toml
[gates]
# All keys optional; absent = strict (maximum gates).
# Only set a key when you intentionally accept a weaker build.

[gates.proof]
# lean = "strict"   # default — target: Lean kernel on lic build
# lean = "off"      # explicit downgrade — local/CI exception; must be reviewed

[gates.security]
# stdlib_seal = true   # default

[gates.performance]
# bench_tier = "ci"    # default — run tier appropriate to package
```

**Environment (documented downgrade, not manifest):**

| Variable | Effect |
|----------|--------|
| `--no-lean-verify` on `lic build` | Skip Lean verify on build (local dev); compiler warns |
| `--allow-open-vc` on `lic build` | Allow open AutoVC goals (tests / emergency only) |

Do not use undocumented env vars to silence gates in CI.

## Optional numerics defaults (not enforced org-wide)

```toml
[numerics]
# Documents this package's preferred scalar widths for agents and humans.
# Does NOT change dependent packages or CI for the whole org.
default_float = "float64"   # or float32, float16, float8, …
default_int = "int64"
```

Callers override in source (`float32` locals) or in physics metadata (`PhysicsProfile.float_bits`). See [Scalar precision](scalar-precision.md).

## Package sections (summary)

See lip § A3 for `[package]`, `[dependencies]`, `[package.metadata.lip]`, `min_coverage`, and workspace `members`.
