# li-std-physics-core

Li package li-std-physics-core

## Scalar precision (per simulation, not org-wide)

Physics tiers and integrator metadata live here. **Float/int width and binary weights are opt-in** via `ScalarPrecision` and `PhysicsProfile.float_bits` / `int_bits` / `weights_encoding`.

| Doc | Content |
|-----|---------|
| [docs/scalar-precision.md](docs/scalar-precision.md) | Package API and presets |
| [lic: scalar-precision.md](https://github.com/li-langverse/lic/blob/main/docs/language/scalar-precision.md) | Full type tables, literal suffixes, `binary` vs `bytes` |

Presets: `precision_default()`, `precision_float32()`, `precision_quantized_fp8()`, `precision_binary_weights()`.

## Build

```bash
lic build src/lib.li -o li-std-physics-core
```

From the monorepo root, ensure `lic` is built: `./scripts/build.sh`.

## Traceability

| ID | Link |
|----|------|
| Package | `PKG-li-std-physics-core` |
| Org repo | https://github.com/li-langverse/li-std-physics-core |
| Governance | [Ecosystem governance](https://li-langverse.github.io/li-language/ecosystem/governance/) |

See `PUBLISH.md` and `docs/traceability.md`.

## License

Apache-2.0 OR MIT
