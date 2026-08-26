---
name: studio-accessibility-web-quality
description: >-
  Accessibility and web-quality checks for Studio surfaces — WCAG-oriented
  contrast, keyboard paths, performance honesty. Use with studio_ui_ux_builder
  and gui_ux_tester.
---

# Studio accessibility & web quality

Adapted from [addyosmani/accessibility](https://officialskills.sh/addyosmani/skills/accessibility) and [web-quality-audit](https://officialskills.sh/addyosmani/skills/web-quality-audit).

## Run harness

```bash
cd li-cursor-agents
python3 ux-harness/run_audit.py --target world-studio-demo --mode both
# CI: add --mock
```

## Checklist (native + mock HTML)

| Area | Check |
|------|--------|
| **Contrast** | Text/icons ≥ WCAG AA on `#0d1117`-style backgrounds |
| **Focus** | Visible focus ring on dock, palette, inspector fields |
| **Keyboard** | Tab order: dock → viewport → inspector → palette |
| **Motion** | Respect `prefers-reduced-motion` for panel transitions |
| **Perf honesty** | Show FPS / particle count when claiming realtime (UX-13) |
| **Loading** | Skeleton or progress for &gt;300ms shell load |

## PH-UX performance gates

From `lic/benchmarks/competitive/studio-ui.toml`:

- Viewport ≥ 60 fps (sustained)
- Panel switch &lt; 100 ms
- MD tiers: 10k@60fps, 100k@30fps targets
- Memory: `./scripts/profile-animate-memory.sh` — document peak MiB

```bash
cd lic
./scripts/bench-studio-viewport-perf.sh
cat data/studio-ui-ux-plan-loop/latest-bench.json
```

## File issues

Only `gui_ux_tester` / `gui_ui_tester` file remediation issues. **Builder** fixes in the same PR when the loop todo is implementation.
