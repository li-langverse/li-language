# RFC: Portable compilation targets (PH-PORT)

**Status:** Draft  
**Track:** PH-PORT  
**Vision:** [world-studio-vision.md](../world-studio-vision.md)

## Proposal

```bash
lic build --target x86_64-unknown-linux-gnu
lic build --target aarch64-apple-darwin
lic build --target-list
```

## Li syntax

Use **`def`** for all new APIs. Do not document bare **`proc`**. **`extern proc`** only for FFI. Every exported `def` (and each `extern proc`) needs `requires` / `ensures` / `decreases`. The parser still accepts legacy bare `proc` in old trees only — reject that syntax in new Studio/game-dev docs and package code.

**`targets/manifest.toml`** — supported triples, CPU features, CI tier (required / optional).

Tier-1 CI: `x86_64-linux`, `aarch64-linux`, `aarch64-darwin`.

## Policy

All official `packages/*` build on tier-1 without source changes.

## Dependencies

LLVM 22 only for CPU; GPU via `[engine.lig]` in `li.toml` (see [lig-rfc.md](lig-rfc.md); legacy `[engine.gpu]` alias one release).
