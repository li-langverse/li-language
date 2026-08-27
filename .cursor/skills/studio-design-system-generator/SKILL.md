---
name: studio-design-system-generator
description: >-
  Generate or refresh Li World Studio design system via ui-ux-pro-max reasoning
  (HUD/Sci-Fi + AI-Native UI). Writes docs/design artifacts and updates HTML mocks.
  Use at start of a UX sprint and when refreshing mocks.
---

# Studio design system generator

Upstream: [ui-ux-pro-max-skill](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill) · [awesome-agent-skills](https://github.com/VoltAgent/awesome-agent-skills).

## Generate (lic)

```bash
cd lic
./scripts/studio-ui-ux-generate-design-system.sh
```

Outputs:

- `docs/design/studio-design-system.generated.md` — full spec (pattern, palette, type, anti-patterns)
- `docs/design/studio-design-tokens.toml` — **approved** tokens for native `li-ui` (edit by hand after review)

## Product brief (use in prompts)

> Li World Studio — scientific 3D IDE with MD/particle viewport, timeline, inspector, command palette, **agent copilot** chrome. Dark OLED base. Style: **HUD / Sci-Fi FUI** + **AI-Native UI**. Avoid generic purple AI gradients and Inter-only slop.

## After generation

1. Apply tokens to `deploy/studio-demo/screenshots/*.html`
2. Map hex → Li palette in `packages/li-ui` when native path exists
3. Re-run `./scripts/studio-ui-ux-capture-progress.sh`
4. Score UX-01…14 — do not claim UX pass if only generator output changed without capture diff

## Manual / agent fallback

If `uipro` / `search.py` missing, the shell script writes a **Li default** spec anchored to `scripts/lib/li-ui.sh` space-tech colors.
