# lig

Li GPU **device layer** (PH-HW **HW-0**): backend identifiers, runtime probe via `li_rt_lig_*`, TOML `backend` line parsing, and wgpu compile-time smoke.

## API

| Symbol | Role |
|--------|------|
| `lig_device_kind()` | Selected backend id (1=cuda, 2=rocm, 3=metal, 4=webgpu) |
| `lig_backend_available(id)` | 1 if env/platform probe sees the backend |
| `lig_backend_select_auto()` | Pick metal/rocm/cuda/webgpu and update selection |
| `lig_capability_json()` | JSON probe snapshot for agents/bench |
| `lig_parse_toml_backend_line(line)` | Parse `[engine.lig] backend = "cuda\|rocm\|metal\|webgpu"` (legacy `[engine.gpu]` one release) |
| `lig_present_surface_ok()` | Honest native surface flag (0 until wgpu-rs presents) |
| `lig_wgpu_smoke_run()` | Smoke struct with `surface_ok = lig_present_surface_ok()` |

## TOML

```toml
[engine.lig]
backend = "webgpu"   # cuda | rocm | metal | webgpu

# legacy (one release):
[engine.gpu]
backend = "metal"
```

## Bench

- `bench/device_probe.toml` — hook for `./scripts/bench-studio-viewport-perf.sh`
- Smoke: `li-tests/smoke/lig_device_probe.li`

## Verify

```bash
lic check packages/lig/li-tests/smoke/lig_device_probe.li
```
