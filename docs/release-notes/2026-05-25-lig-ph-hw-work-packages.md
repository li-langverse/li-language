# PH-HW WP1: `lig` governance, kernel catalog, multi-vendor bench schema

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** PH-HW **WP1** (HW-0 governance); targets branch `feat/ph-hw-multi-vendor`

---

## Summary

Documents the **`lig`** GPU package (`import lig`), LKIR layout, kernel catalog IDs, and `lig-kernels.toml` multi-vendor bench columns — docs and benchmarks only; no `packages/lig/src/lib.li` implementation (WP2+).

## Agent continuation

1. Read: `docs/game-dev/specs/lig-rfc.md`, `docs/game-dev/lig-kernel-catalog.md`, `benchmarks/competitive/lig-kernels.toml`.
2. Run: `rg 'import gpu|li-gpu' docs/` (should only hit redirect/migration text); `./scripts/check-hpc-competitive.sh` if extended for lig-kernels in WP2.
3. Then: WP2 — `git mv packages/li-gpu packages/lig` (or steward), implement `lig.device|present|kernel|memory|custom` in `packages/lig/src/lib.li`.
4. Blocked on: vendor lowering and timed fills in `lig-kernels.toml` until WP2 harness lands.

## Changed

| Area | What | Evidence |
|------|------|----------|
| RFC | Full `lig-rfc.md`; `li-gpu-lkir-rfc.md` redirect | `docs/game-dev/specs/` |
| Catalog | Stable `lig.kernel.*` IDs | `docs/game-dev/lig-kernel-catalog.md` |
| Benches | Schema `cpu\|cuda\|hip\|metal\|custom_lab` | `benchmarks/competitive/lig-kernels.toml` |
| Vision | `lig` replaces `gpu` / `li-gpu` | `docs/game-dev/world-studio-vision.md`, `PH-world-studio-program.md` |
| Ecosystem | Package register + P1 order | `docs/ecosystem/algorithms-and-libraries-plan.md` |
| Portable targets | `[engine.lig]` | `docs/game-dev/specs/portable-targets-rfc.md` |

## Not changed

- `packages/lig/src/lib.li` or LKIR lowering codegen — **WP2+**.
- `lic` compiler GPU backend emission — no codegen edits in WP1.
- `benchmarks/competitive/registry.toml` tier-1/2 CPU rows — separate harness.
- Studio viewport runtime — WP3 (`lig.present` wire-up).
- `provability-gaps.md`, master plan phase order — not edited.

## Breaking changes

None shipped (documentation only). Future breaking: `import gpu` removal after alias period (documented in lig-rfc.md).

## Security

N/A — no trusted surface or FFI in this PR. WP2 must gate `lig.custom` / `custom_lab` manifest per lig-rfc security gate.

## Performance

N/A — `lig-kernels.toml` rows are **stub** (empty vendor columns). No dashboard ratio claims until WP2 ingest.

## Downstream

| Repo | Action |
|------|--------|
| benchmarks dashboard | Ingest `lig-kernels.toml` when WP2 fills timings |
| studio / render | Read lig-rfc before present path PRs |

## CHANGELOG entry

```markdown
### Added
- **PH-HW WP1:** `lig` RFC, kernel catalog, `lig-kernels.toml` multi-vendor schema — `docs/release-notes/2026-05-25-lig-ph-hw-work-packages.md`.
```
