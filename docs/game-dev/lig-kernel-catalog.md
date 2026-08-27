# `lig` kernel catalog (PH-HW WP1)

**Status:** Governance (IDs + contracts; implementation WP2+)  
**RFC:** [specs/lig-rfc.md](specs/lig-rfc.md)  
**Bench registry:** [lig-kernels.toml](../../benchmarks/competitive/lig-kernels.toml)

Stable kernel IDs use the prefix **`lig.kernel.`**. Agents add rows here **and** in `lig-kernels.toml` before claiming perf parity.
**Vendor columns (honest stubs):** `cuda`, `hip`, and `metal` are **`N/A`** until emit env (`LIG_EMIT_CUDA=1`, `LIG_EMIT_HIP=1`, `LIG_EMIT_METAL=1`). `./scripts/bench-lig-kernel-parity.sh` emits all catalog `kernel_ids` in JSON.

## Naming

| Pattern | Example |
|---------|---------|
| `lig.kernel.<name>_<dtype>` | `lig.kernel.matmul_f32` |
| Physics shorthand | `lig.kernel.md_force_short` |
| Present / copy | `lig.kernel.memcpy_h2d_f32` |

## Catalog

| Kernel ID | Domain | Host contract (summary) | LKIR module | Bench row |
|-----------|--------|-------------------------|-------------|-----------|
| `lig.kernel.matmul_f32` | LA | `a.cols == b.rows`; aligned buffers | `lkir/matmul_tile.lkir` | `matmul_f32` |
| `lig.kernel.matmul_f64` | LA | same shape rules, f64 | `lkir/matmul_tile_f64.lkir` | `matmul_f64` |
| `lig.kernel.dot_f32` | LA | `a.len == b.len` | `lkir/dot_warp.lkir` | `dot_f32` |
| `lig.kernel.reduce_sum_f32` | LA | 1-D contiguous | `lkir/reduce_sum.lkir` | `reduce_sum_f32` |
| `lig.kernel.md_force_short` | MD | `r.len > 0`; cutoff in params | `lkir/md_lj_short.lkir` | `md_force_short` |
| `lig.kernel.md_force_full` | MD | same + full neighbor list | `lkir/md_lj_full.lkir` | `md_force_full` |
| `lig.kernel.heat_stencil_2d_f32` | PDE | grid dims match halo | `lkir/heat_2d.lkir` | `heat_stencil_2d` |
| `lig.kernel.sph_density_f32` | CFD | particle count > 0 | `lkir/sph_density.lkir` | `sph_density` |
| `lig.kernel.horner_f32` | micro | coeffs.len > 0 | `lkir/horner.lkir` | `horner_f32` |
| `lig.kernel.mlp_forward_f32` | ML | layer dims consistent | `lkir/mlp_tile.lkir` | `mlp_forward_f32` |
| `lig.kernel.memcpy_h2d_f32` | mem | `dst.len == src.len` | `lkir/memcpy.lkir` | `memcpy_h2d` |
| `lig.kernel.memcpy_d2h_f32` | mem | same | `lkir/memcpy.lkir` | `memcpy_d2h` |
| `lig.kernel.quat_rotate_vec3` | graphics | unit quaternion | `lkir/quat_rotate.lkir` | stub |
| `lig.kernel.present_blit_rgba8` | present | surface valid | `lkir/blit.lkir` | stub |

## Launch API (WP2)

```li
import lig.kernel

def launch_matmul(a: tensor[f32], b: tensor[f32]) -> tensor[f32]
  requires a.cols == b.rows
  requires lig.memory.aligned(a) && lig.memory.aligned(b)
=
  lig.kernel.launch(id = "lig.kernel.matmul_f32", args = [a, b])
```

## Gate checklist (per kernel)

| Gate | Catalog field | Bench field |
|------|---------------|-------------|
| Validity | `validity` in TOML | `validity_ref` |
| Security | `trusted_ffi` flag | N/A |
| Memory | `memory_contract` | sanitizer smoke id |
| Performance | `perf_tier` | `cpu` stub/pilot; `cuda`/`hip`/`metal` = `N/A` until emit env |

## Adding a kernel

1. Allocate `lig.kernel.<name>` here.  
2. Add `[[kernel]]` row to `benchmarks/competitive/lig-kernels.toml`.  
3. Land LKIR under `packages/lig/lkir/` (WP2).  
4. Wire `li-tests/lig/<id>.li` smoke (WP2).
