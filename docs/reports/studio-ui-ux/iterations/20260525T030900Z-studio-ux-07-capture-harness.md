# Iteration studio-ux-07-capture-harness

**UTC:** 2026-05-25T03:09:00Z  
**Branch:** cursor/studio-ui-ux-plan-loop

## Shipped

- `scripts/studio-ui-ux-verify-capture.py` — dry capture + gh release path check
- `deploy/studio-demo/screenshots/03-studio-agent-error.html` + `manifest.json` + README
- `studio-ui-ux-capture-progress.sh` — `latest-capture.json`, issue comment URL, label fallback
- Gates require verify-capture + third mock

## Capture

- **exit:** 0
- **issue comment:** https://github.com/li-langverse/lic/issues/182#issuecomment-4531186566
- **release:** https://github.com/li-langverse/lic/releases/tag/studio-ui-ux-progress (3 PNG + reel)
- **artifacts:** `data/studio-ui-ux-plan-loop/artifacts/iter-studio-ux-07-capture-harness/`

## Bench (`latest-bench.json`)

- `load_ms`: 0.1
- `md_particles`: 1k/10k/100k @ 60/60/30 fps (simulate)
- `memory_mib`: tracemalloc peak ~1.07 MiB
