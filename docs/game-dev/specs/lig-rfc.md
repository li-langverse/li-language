# RFC: `lig` — Li GPU package, LKIR, multi-vendor kernels (PH-HW)

**Status:** Draft (WP1 governance)  
**Track:** PH-HW · HW-0 (governance)  
**Vision:** [world-studio-vision.md](../world-studio-vision.md)  
**Kernel catalog:** [lig-kernel-catalog.md](../lig-kernel-catalog.md)  
**Bench schema:** [lig-kernels.toml](../../../benchmarks/competitive/lig-kernels.toml)

## Summary

Rename and unify GPU work under **`lig`** (`import lig`), replacing the provisional **`li-gpu`** package and legacy **`import gpu`** surface. User kernels compile through **LKIR** (Li Kernel IR) under `packages/lig/lkir/` and lower to CUDA, HIP/ROCm, Metal, SPIR-V/wgpu, or a **custom lab** backend — never raw vendor source strings in application code.

**WP1 (this RFC):** governance, module boundaries, vendor matrix, four proof/bench gates, migration plan.  
**WP2+:** `packages/lig/src/lib.li`, codegen, and harness drivers.

## Package layout

| Path | Role |
|------|------|
| `packages/lig/` | Published package `lig` (workspace member) |
| `packages/lig/lkir/` | LKIR schema, opcode tables, vendor lowering tables |
| `packages/lig/src/lib.li` | Public API (`lig.device`, `lig.present`, …) — **WP2** |
| `benchmarks/competitive/lig-kernels.toml` | Multi-vendor timing registry |

**Integration steward / WP2** performs the mechanical rename:

```bash
git mv packages/li-gpu packages/lig   # when li-gpu exists on the integration branch
```

Until the mv lands, docs and benches may reference `packages/gpu` stubs; agents must treat **`lig`** as the canonical name.

## Module surface

| Module | Responsibility |
|--------|----------------|
| `lig.device` | Enumerate devices, queues, capabilities, backend selection |
| `lig.present` | Surfaces, swapchains, frame pacing (viewport path via wgpu/Metal) |
| `lig.kernel` | Catalog kernel launch, LKIR module load, autotune hooks (PH-ML) |
| `lig.memory` | Device allocations, host↔device copies, alignment contracts |
| `lig.custom` | Opt-in **custom_lab** backend for university/lab drivers (gated) |

### Li syntax

Use **`def`** for all new APIs. Do not document bare **`proc`**. **`extern proc`** only for FFI to trusted vendor runtimes. Every exported `def` needs `requires` / `ensures` / `decreases` where applicable.

```li
import lig
import lig.device
import lig.kernel

def matmul_f32(a: tensor[f32], b: tensor[f32]) -> tensor[f32]
  requires a.cols == b.rows
  requires lig.memory.aligned(a) && lig.memory.aligned(b)
  ensures result.rows == a.rows && result.cols == b.cols
=
  lig.kernel.launch(id = "lig.kernel.matmul_f32", args = [a, b])

def pick_backend() -> lig.device.Backend
  requires lig.device.probe_ok()
=
  lig.device.select(prefer = ["metal", "cuda", "hip", "wgpu"])
```

```li
import lig.present

def viewport_frame(surface: lig.present.Surface) -> ()
  requires surface.valid()
=
  lig.present.begin(surface)
  lig.present.submit()
  lig.present.end(surface)
```

```toml
# li.toml (engine / studio project)
[engine.lig]
backend = "auto"   # auto | cuda | hip | metal | wgpu | custom_lab
lkir_path = "kernels/my_tile.lkir"
custom_lab_manifest = ""   # required when backend = custom_lab
```

## LKIR (Li Kernel IR)

LKIR is the **only** supported IR for user-defined GPU kernels.

| Property | Rule |
|----------|------|
| Location | `packages/lig/lkir/` — versioned schema + worked examples |
| Tile semantics | Explicit tile sizes, memory scopes, sync barriers |
| Lowering | LKIR → CUDA PTX/SASS path, HIP, Metal AIR, SPIR-V (wgpu), custom lab stub |
| Forbidden | CUDA/HIP/Metal **source strings** in `.li` user modules |
| Triton interop | Optional import/export for autotune (PH-ML); not a user-facing default |

Example catalog binding (conceptual):

```li
import lig.kernel

def md_force_short(r: tensor[f32], eps: f32, sigma: f32) -> tensor[f32]
  requires r.len > 0
  requires eps > 0.0 && sigma > 0.0
=
  lig.kernel.launch(id = "lig.kernel.md_force_short", args = [r, eps, sigma])
```

## Vendor matrix

| Vendor | Backend id | Primary API | Studio viewport | HPC / ML kernels | CI smoke |
|--------|------------|-------------|-----------------|------------------|----------|
| Apple | `metal` | Metal 3 | **default** on macOS | LKIR → Metal | macOS runner |
| NVIDIA | `cuda` | CUDA 12+ | via wgpu fallback | LKIR → CUDA | Linux + GPU label |
| AMD | `hip` | ROCm / HIP | via wgpu fallback | LKIR → HIP | `hipcc` probe, gfx arch |
| Cross-vendor | `wgpu` | wgpu / Vulkan / DX12 | **default** on Windows/Linux | subset kernels | headless present smoke |
| Lab / research | `custom_lab` | Site manifest | off by default | gated catalog IDs | manual only |

**Parity rule:** ROCm/HIP is a **peer** of CUDA, not a secondary port. No CUDA-only user kernels.

## Four gates (every kernel + backend change)

| Gate | What it proves / measures | Evidence |
|------|---------------------------|----------|
| **Validity** | LKIR well-formed; launch preconditions; numeric bounds vs CPU reference | `lic check`, `li-tests/lig/*`, catalog `validity` column |
| **Security** | No new trusted axioms without review; `custom_lab` manifest hash; no arbitrary driver load | CVE row if FFI touched; `lig.custom` allowlist |
| **Memory** | No OOB, alias, or leak on copy/alloc paths | `lig.memory` contracts + sanitizer smokes |
| **Performance** | Timed row in `lig-kernels.toml`; ratio vs CPU oracle ≤ policy | [benchmarks dashboard](https://li-langverse.github.io/benchmarks/) |

Agents **must not** weaken these gates in prompts, hooks, or bench thresholds.

## Migration from `li-gpu` / `import gpu`

| Legacy | Replacement |
|--------|-------------|
| Package `li-gpu` / folder `packages/li-gpu` | `lig` / `packages/lig` (`git mv`) |
| `import gpu` | `import lig` |
| `gpu.cuda`, `gpu.rocm`, `gpu.wgpu` | `lig.device` + `lig.kernel` |
| [li-gpu-lkir-rfc.md](li-gpu-lkir-rfc.md) | Stub → this RFC |
| `[engine.gpu]` in `li.toml` | `[engine.lig]` (alias accepted one release) |

```li
# Before (deprecated)
# import gpu
# def run() = gpu.cuda.matmul(a, b)

# After
import lig
import lig.kernel

def run(a: tensor[f32], b: tensor[f32]) -> tensor[f32]
  requires a.cols == b.rows
=
  lig.kernel.launch(id = "lig.kernel.matmul_f32", args = [a, b])
```

## PH-HW work packages

| WP | Scope | Owner |
|----|-------|-------|
| **WP1** | This RFC, kernel catalog, `lig-kernels.toml`, vision doc alignment | Docs + benchmarks |
| **WP2** | `packages/lig/src/lib.li`, LKIR parser/lowering smoke | Implementation |
| **WP3** | `lig.present` + Studio viewport wire-up | Studio + render |
| **WP4** | ROCm/HIP CI parity, multi-GPU overlap (PH-ML) | HW + ML |

## Proof / trust

| Layer | Status |
|-------|--------|
| Host launch (`requires` on buffers, alignment) | Target: proved in Lean via `lic build` |
| Device kernel bodies | Trusted until device calculus lands; LKIR carries ghost specs |
| Vendor FFI (`extern proc`) | Trusted + SBOM; **IMPORTANT** tier per [critical-package-compliance-rfc.md](critical-package-compliance-rfc.md) |

## Dependencies

- [PH-world-studio-program.md](../PH-world-studio-program.md) — PH-HW HW-0…4
- [portable-targets-rfc.md](portable-targets-rfc.md) — CPU triples + `[engine.lig]`
- [ml-async-parallel-rfc.md](ml-async-parallel-rfc.md) — streams, Triton-distributed patterns
- [algorithms-and-libraries-plan.md](../../ecosystem/algorithms-and-libraries-plan.md) — package register

## Open questions

- [ ] Exact LKIR textual syntax (`.lkir` vs embedded `.li` attributes) — WP2 spike
- [ ] wgpu vs Metal default on Apple Silicon when both available
- [ ] External oracle binaries for `md_force_short` (LAMMPS column) — see `lig-kernels.toml`
