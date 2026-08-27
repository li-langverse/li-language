# RFC: Additive manufacturing + export to printer (PH-AM)

**Status:** Draft  
**Track:** PH-AM  
**Vision:** [world-studio-vision.md](../world-studio-vision.md)

## Proposal

**Simulate and manufacture** on Li Engine:

1. Mesh + voxel occupancy (`li-voxel` mode `am_occupancy`)  
2. Thermal/warp (`heat_equation`, proved tolerance)  
3. **`sim.export.print`** → STL, 3MF, G-code  
4. Optional send: OctoPrint / PrusaLink / Bambu (trusted FFI)

```toml
[engine.export]
formats = ["3mf", "gcode"]
require_sim_pass = true
printer_profile = "profiles/bambu_x1c.toml"
```

**Export wizard (PH-UX):** Review → Pre-flight → Export/Print (≤3 clicks).

## Li syntax

Use **`def`** for all new APIs. Do not document bare **`proc`**. **`extern proc`** only for FFI. Every exported `def` (and each `extern proc`) needs `requires` / `ensures` / `decreases`. The parser still accepts legacy bare `proc` in old trees only — reject that syntax in new Studio/game-dev docs and package code.

## Domains

`fdm` | `sla` | `sls` | `ded` | `metal_am`

## Compliance

`li-sim-additive` tier **CRITICAL** — export audit log (PH-COMPLY).
