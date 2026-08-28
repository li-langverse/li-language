#!/usr/bin/env bash
# Design system for Li World Studio — ui-ux-pro-max search.py or Li default.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_MD="${ROOT}/docs/design/studio-design-system.generated.md"
QUERY="${STUDIO_DS_QUERY:-scientific 3D IDE viewport agent copilot HUD sci-fi dark}"
PRODUCT="${STUDIO_DS_PRODUCT:-Li World Studio}"

mkdir -p "${ROOT}/docs/design"

run_uipro_search() {
  local skill_dir=""
  for d in \
    "${ROOT}/.cursor/skills/ui-ux-pro-max" \
    "${HOME}/.cursor/skills/ui-ux-pro-max" \
    "${ROOT}/.claude/skills/ui-ux-pro-max"; do
    if [[ -f "${d}/scripts/search.py" ]]; then
      skill_dir="$d"
      break
    fi
  done
  if [[ -z "$skill_dir" ]]; then
    if command -v uipro >/dev/null 2>&1; then
      (cd "$ROOT" && uipro init --ai cursor --offline 2>/dev/null || true)
      for d in "${ROOT}/.cursor/skills/ui-ux-pro-max" "${HOME}/.cursor/skills/ui-ux-pro-max"; do
        [[ -f "${d}/scripts/search.py" ]] && skill_dir="$d" && break
      done
    fi
  fi
  if [[ -n "$skill_dir" ]]; then
    python3 "${skill_dir}/scripts/search.py" "$QUERY" --design-system -f markdown -p "$PRODUCT" \
      >"$OUT_MD" 2>/dev/null && return 0
  fi
  return 1
}

write_li_default() {
  cat >"$OUT_MD" <<'EOF'
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
EOF
}

if run_uipro_search; then
  echo "studio-ui-ux-generate-design-system: wrote $OUT_MD (ui-ux-pro-max)"
else
  write_li_default
  echo "studio-ui-ux-generate-design-system: wrote $OUT_MD (Li default)"
fi

python3 "${ROOT}/scripts/studio-ui-ux-emit-demo-css.py"
STAMP="${ROOT}/data/studio-ui-ux-plan-loop/design-system-stamp.txt"
mkdir -p "$(dirname "$STAMP")"
date -u +%Y-%m-%dT%H:%M:%SZ >"$STAMP"
echo "studio-ui-ux-generate-design-system: tokens → deploy/studio-demo/screenshots/studio-tokens.css"
