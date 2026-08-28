# Li World Studio — design system (Li default)

_Generated when ui-ux-pro-max search.py is unavailable. Product: scientific 3D IDE + agent copilot._

## Pattern
- **Editor shell:** dock + topbar + central viewport + right inspector + bottom timeline
- **Agentic:** persistent task strip (idle/running/error), cancel, last action summary

## Style
- **HUD / Sci-Fi FUI** on **dark OLED** (`#0d1117`)
- **AI-Native UI:** clear tool progress, no purple gradient slop

## Colors (from studio-design-tokens.toml)
- Primary bg `#0d1117`, elevated `#161b22`, border `#30363d`
- Accents: cyan `#3dd6ff`, violet `#7c5cff`, mint `#2ee6a8`
- Agent running `#238636`, error `#da3633`

## Typography
- UI: Inter / system-ui; mono for metrics and logs

## Motion
- Panel switch ≤ 100ms; hover 150–200ms; respect `prefers-reduced-motion`

## Anti-patterns
- Generic AI purple/pink gradients, fake glass without contrast
- Empty viewport with no empty state; marketing mock labeled as native

## Pre-delivery
- [ ] Contrast AA on text
- [ ] Focus visible on palette and dock
- [ ] FPS / particle count visible when claiming realtime
