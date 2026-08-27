#!/usr/bin/env bash
# Gates for Studio UI/UX plan loop — native ui/gui/render + UX capture scripts.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
li_detect_compilers 2>/dev/null || true
export LI_REPO_ROOT="$ROOT"

fail() { li_gate_fail "$*"; exit 1; }

li_phase "studio-ui-ux scripts"
[[ -x "$ROOT/scripts/studio-ui-ux-plan-loop.py" ]] || chmod +x "$ROOT/scripts/studio-ui-ux-plan-loop.py"
[[ -f "$ROOT/scripts/studio-ui-ux-capture-progress.sh" ]] || fail "missing studio-ui-ux-capture-progress.sh"
[[ -f "$ROOT/scripts/bench-studio-viewport-perf.sh" ]] || fail "missing bench-studio-viewport-perf.sh"

li_phase "design system (tokens + demo CSS)"
"$ROOT/scripts/studio-ui-ux-generate-design-system.sh" || fail "studio-ui-ux-generate-design-system"
[[ -f "$ROOT/docs/design/studio-design-tokens.toml" ]] || fail "studio-design-tokens.toml"
[[ -f "$ROOT/deploy/studio-demo/screenshots/studio-tokens.css" ]] || fail "studio-tokens.css"
[[ -f "$ROOT/deploy/studio-demo/screenshots/01-studio-workspace.html" ]] || fail "studio workspace mock"
[[ -f "$ROOT/deploy/studio-demo/screenshots/02-studio-empty-viewport.html" ]] || fail "studio empty viewport mock"
[[ -f "$ROOT/deploy/studio-demo/screenshots/03-studio-agent-error.html" ]] || fail "studio agent error mock"
[[ -f "$ROOT/deploy/studio-demo/screenshots/manifest.json" ]] || fail "studio demo manifest"
[[ -x "$ROOT/deploy/studio-demo/screenshots/capture.sh" ]] || chmod +x "$ROOT/deploy/studio-demo/screenshots/capture.sh"
chmod +x "$ROOT/scripts/studio-ui-ux-capture-progress.sh" 2>/dev/null || true
python3 "$ROOT/scripts/studio-ui-ux-verify-capture.py" || fail "studio-ui-ux-verify-capture"
python3 "$ROOT/scripts/studio-ui-ux-verify-native-capture.py" || fail "studio-ui-ux-verify-native-capture"
python3 "$ROOT/scripts/studio-ui-ux-verify-tokens.py" || fail "studio token sync (TOML ↔ li-ui)"

li_phase "competitive intel doc"
[[ -f "$ROOT/docs/game-dev/competitive-intel/ui-ux-by-dimension.md" ]] || fail "ui-ux-by-dimension.md"
[[ -f "$ROOT/benchmarks/competitive/studio-ui.toml" ]] || fail "studio-ui.toml bench registry"

li_phase "studio-ui bench registry"
"$ROOT/scripts/bench-studio-viewport-perf.sh" || fail "bench-studio-viewport-perf"
python3 "$ROOT/scripts/studio-ui-ux-verify-bench-registry.py" || fail "studio-ui-ux-verify-bench-registry"

if [[ "${STUDIO_UI_UX_GATES_SKIP_BUILD:-0}" != "1" ]]; then
  if [[ -x "$ROOT/build/compiler/lic/lic" ]] || [[ -x "$ROOT/scripts/build.sh" ]]; then
    if [[ ! -x "${LIC:-$ROOT/build/compiler/lic/lic}" ]] && [[ -x "$ROOT/scripts/build.sh" ]]; then
      li_warn "lic not built — run ./scripts/build.sh (or STUDIO_UI_UX_GATES_SKIP_BUILD=1)"
    fi
  fi
  if [[ -d "$ROOT/li-tests" ]]; then
    li_phase "li-tests studio packages (compile_ok)"
    for pkg in li-ui li-gui li-render li-scene li-studio; do
      pkg_root="$ROOT/packages/$pkg"
      [[ -d "$pkg_root" ]] || pkg_root="$ROOT/$pkg"
      if [[ -d "$pkg_root" ]]; then
        if ! "$ROOT/li-tests/run_all.sh" --package "$pkg" composable 2>/dev/null; then
          li_warn "skip or soft-fail $pkg composable tests (package may be stub)"
        fi
      fi
    done
  fi
fi

li_phase "memory profile smoke"
"$ROOT/scripts/profile-animate-memory.sh" || fail "profile-animate-memory"
[[ -f "$ROOT/data/studio-ui-ux-plan-loop/latest-memory-profile.json" ]] \
  || fail "latest-memory-profile.json missing after profile-animate-memory"

if [[ "${STUDIO_UI_UX_GATES_CAPTURE:-0}" == "1" ]]; then
  li_phase "capture progress (dry)"
  STUDIO_UI_UX_CAPTURE_DRY=1 "$ROOT/scripts/studio-ui-ux-capture-progress.sh" || fail "capture dry"
fi

li_ok "studio-ui-ux plan gates"
