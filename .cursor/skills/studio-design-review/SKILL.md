---
name: studio-design-review
description: >-
  Visual UI review for Li Studio — capture screenshots/video each iteration,
  score dimensions 0–3, detect regressions and generic AI-slop. Use with
  studio_ui_ux_builder and gui_ui_tester.
---

# Studio design review

Adapted from community patterns ([design-review](https://officialskills.sh/garrytan/skills/design-review), [plan-design-review](https://officialskills.sh/garrytan/skills/plan-design-review)).

## Every iteration (mandatory)

```bash
cd lic
./scripts/studio-ui-ux-capture-progress.sh
# STUDIO_UI_UX_ITERATION=<todo-id> set by plan loop
```

Artifacts go to GitHub issue + release `studio-ui-ux-progress` — **never** commit PNG/MP4.

## Review workflow

1. **Capture** — native viewport when available; else `deploy/studio-demo/screenshots/` HTML mocks (label as mock in PR).
2. **Compare** — prior iteration screenshots on the GitHub release or issue thread.
3. **Score** — UX-01…UX-14 per `lic/docs/game-dev/competitive-intel/ui-ux-by-dimension.md` (0–3 + one line each).
4. **Anti-patterns** — flag: purple gradients, generic Inter-only chrome, empty viewport with no empty state, fake metrics, marketing mock presented as native.
5. **Fix** — smallest shippable UI diff; one logical change per commit when possible.

## Output (PR body)

```markdown
### Design review
| Dimension | Score | Note |
|-----------|------:|------|
| UX-01 | 2 | … |
…
**Regressions:** none | UX-04 dropped 3→2 (palette latency)
**Capture:** issue #N comment / release asset names
```

## Do not

- Skip capture because render is stubbed
- Weaken bench gates to greenwash UX scores
