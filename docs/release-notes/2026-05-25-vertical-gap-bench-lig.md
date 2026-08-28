release note written
udio-ui bench → lig

**Summary:** Point PH-UX studio viewport bench registry and harness at `packages/lig` (rename from `li-gpu`) so `wgpu_smoke` hooks and `latest-bench.json` report `present:lig`.

## Agent continuation

1. **Read** — `benchmarks/competitive/studio-ui.toml`, `packages/lig/bench/wgpu_smoke.toml`, `scripts/bench-studio-viewport-perf.sh`.
2. **Run** — `./scripts/profile-animate-memory.sh`; `./scripts/bench-studio-viewport-perf.sh`; `python3 scripts/studio-ui-ux-verify-bench-registry.py`; `lic check packages/lig/li-tests/smoke/lig_device_probe.li`.
3. **Then** — PH-HW follow-up: fix `import lig.present` resolver for `wgpu_smoke.li` on `main` (separate PR); wire real `lig_present_surface_ok()` when host present.
4. **Blocked on** — none for registry path migration.

## Changed

| Area | What | Evidence |
|------|------|----------|
| Registry | `[[hook]] wgpu_smoke` → `package = "lig"`, `packages/lig/bench/wgpu_smoke.toml` | `studio-ui-ux-verify-bench-registry.py` ok |
| Harness | Drop `li-gpu` fallback; cold-load scan lists `lig` | `./scripts/bench-studio-viewport-perf.sh` → `hooks.wgpu_smoke.present=true`, `notes` includes `present:lig` |
| Bench JSON | Regenerated `latest-bench.json` / competitive copy (gitignored) | `registry_schema`: `li_studio_ui_bench_v1` |
| Smoke | HW-0 device probe | `lic check packages/lig/li-tests/smoke/lig_device_probe.li` exit 0 |

## Not changed

- `packages/lig/present/lib.li` or `wgpu_smoke.li` import resolution (still fails `lic check` on `main`).
- Real wgpu-rs surface / `native_pixels` host path.
- `li-render` / `li-studio` package APIs beyond bench registry paths.
- Particle tier simulate semantics or gate targets.

## Breaking

None — registry consumers should read `lig` hook paths; `li-gpu` paths removed from studio-ui bench only.

## Security

N/A — bench metadata only.

## Performance

N/A — simulate harness unchanged; reproduce with `./scripts/bench-studio-viewport-perf.sh`.

## Downstream

| Repo | Action |
|------|--------|
| li-cursor-agents / studio-ui-ux plan loop | Re-run bench after merge; refresh gitignored `latest-bench.json` locally |
| benchmarks dashboard | N/A until competitive JSON published from CI |
