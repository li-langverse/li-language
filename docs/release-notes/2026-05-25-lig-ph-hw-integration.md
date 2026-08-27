# PH-HW integration: `lig` multi-vendor GPU + studio gap rollup

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** integration `feat/ph-hw-multi-vendor` → `main`  
**PH / REQ:** PH-HW (HW-0…4), PH-AGENT / AGENT-0, PH-SIM SIM-0, PH-GD-1/2  
**Author:** agent

---

## Summary (one sentence)

Single integration branch and release note for **`lig`** multi-vendor GPU work packages (WP1–WP5) stacked on **`studio-gap-close-wave1`** (World Studio UX, MCP contracts, sim profile bridge) before merge to `main`.

## Agent continuation (required)

1. Read: this file; `docs/game-dev/specs/lig-rfc.md`; child notes under `docs/release-notes/2026-05-25-{studio,lig,ph-hw}*`; stack PRs [#217](https://github.com/li-langverse/lic/pull/217)–[#222](https://github.com/li-langverse/lic/pull/222) and [#213](https://github.com/li-langverse/lic/pull/213) (WP3).
2. Run: merge WP branches into `feat/ph-hw-multi-vendor` in order **WP1 → WP2 → WP3 → WP4 → WP5** (resolve `CHANGELOG.md` if needed); then `./scripts/ci.sh` or `npm run ci:local` profile for `packages/lig` + `packages/li-studio` smokes.
3. Then: open/update integration PR to `main`; human review — **do not self-merge**.
4. Blocked on: WP PR CI green and merge into integration branch; rebase onto latest `main` if `feat/ph-hw-multi-vendor` lags after other merges land.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| **PH-HW WP1** | `lig` RFC, kernel catalog, `lig-kernels.toml` schema | [#217](https://github.com/li-langverse/lic/pull/217) · [2026-05-25-lig-ph-hw-work-packages.md](2026-05-25-lig-ph-hw-work-packages.md) |
| **PH-HW WP2** | `packages/lig` `lig.device` probe, `li-gpu` → `lig` rename | [#218](https://github.com/li-langverse/lic/pull/218) · `docs/release-notes/2026-05-25-ph-hw-lig-device.md` (on WP branch) |
| **PH-HW WP3** | `lig.present` swapchain + host present / studio glue | [#213](https://github.com/li-langverse/lic/pull/213) · `docs/release-notes/2026-05-25-lig-present-hw1.md` (on WP branch) |
| **PH-HW WP4** | `lig.kernel` LKIR matmul pilot, parity bench | [#220](https://github.com/li-langverse/lic/pull/220) · `docs/release-notes/2026-05-25-ph-hw-wp4-lig-kernel-lkir.md` (on WP branch) |
| **PH-HW WP5** | `lig.memory` / `lig.custom` HW-3/4 | [#222](https://github.com/li-langverse/lic/pull/222) · `docs/release-notes/2026-05-25-lig-memory-custom-hw34.md` (on WP branch) |
| **Studio gap** | UX-01–13, MCP tool IDs, sim profile bridge, li-world scaffold, viewport error overlay | merge `studio-gap-close-wave1` · notes `2026-05-25-studio-*.md`, `2026-05-25-li-world-scaffold.md` |

### Studio gap summary (on integration branch)

| Track | Delivered (interface / smoke) |
|-------|-------------------------------|
| UX-01/13 | Selection + FPS HUD (`li-gui` / `li-render`) |
| UX-02/03 | Timeline playback + inspector fields (`li-studio` v5) |
| UX-04/07 | Command palette + empty states |
| UX-06/08 | Agent chrome + viewport error overlay |
| UX-09/10/11 | Keyboard bindings, SDL input stub, focus/loading chrome |
| AGENT-0 | MCP tool ID contracts + smokes |
| SIM-0 | Studio profile → `li-sim` read-only bridge |
| PH-GD-1/2 | Headless shell demo frame, scene outliner, `li-world` text scaffold |

## Not changed (scope fence)

- **`lic` compiler Wave A** (2e/2f/7b proof gates) — domain libs remain stub/composable per `docs/ecosystem/algorithms-and-libraries-plan.md`.
- **Full CUDA / HIP / Metal LKIR lowering emit** — host/runtime stubs and catalog only until post-WP harness fills `lig-kernels.toml`.
- **`lig.custom` / custom_lab production drivers** — manifest-gated lab path; no arbitrary driver load in this integration.

## Breaking changes

None until WP2+ merges: future **`import gpu` / `li-gpu`** removal documented in `lig-rfc.md`. Studio `li_std_studio_version >= 5` for new smokes after gap merge.

## Security

| Gate | Status |
|------|--------|
| **Validity** | `lic check` on `packages/lig` / `li-studio` smokes per merged WP; LKIR well-formedness on WP4 |
| **Security** | `lig.custom` allowlist + CVE row if FFI touched (WP5); MCP contracts only on studio slice |
| **Memory** | `lig.memory` contracts + MD tier refusal (WP5); studio compose-only paths N/A |
| **Performance** | `lig-kernels.toml` stub columns until harness ingest; studio UX benches optional |

## Performance

N/A for integration rollup — no new dashboard ratios until WP4 bench rows are filled. Studio competitive JSON output (UX-08) is harness-only.

## Downstream

| Repo | Action |
|------|--------|
| benchmarks | Ingest `lig-kernels.toml` when WP4 timings land |
| studio / render | Consume `lig.present` after WP3 merge |
| lip / lit | N/A until `packages/lig` published |

## CHANGELOG entry (paste into Unreleased)

```markdown
### Added
- **PH-HW integration:** `lig` multi-vendor GPU stack (WP1–WP5 child PRs) + studio gap wave on `feat/ph-hw-multi-vendor` — [2026-05-25-lig-ph-hw-integration.md](docs/release-notes/2026-05-25-lig-ph-hw-integration.md).
```
