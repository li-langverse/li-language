# RFC stub: studio-ux-design-system-rfc

**Status:** Draft stub  
**Date:** 2026-05  
**Vision:** [world-studio-vision.md](../world-studio-vision.md)

## Problem

<!-- TODO: one paragraph -->

## Proposal

<!-- TODO: API, packages, phases -->

## Li syntax

Use **`def`** for all new APIs. Do not document bare **`proc`**. **`extern proc`** only for FFI. Every exported `def` (and each `extern proc`) needs `requires` / `ensures` / `decreases`. The parser still accepts legacy bare `proc` in old trees only — reject that syntax in new Studio/game-dev docs and package code.


## Proof / trust

<!-- TODO: what is proved vs trusted -->

## Dependencies

See [PH-world-studio-program.md](../PH-world-studio-program.md).

## Open questions

- [ ] …

