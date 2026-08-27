# Li terminal UI — space-tech palette (respects NO_COLOR, plain when not a TTY).
# shellcheck shell=bash
# Usage: source "$(dirname "$0")/lib/li-ui.sh"  (from scripts/) or source "$ROOT/scripts/lib/li-ui.sh"

_li_ui_init() {
  if [[ -n "${LI_UI_INIT:-}" ]]; then
    return
  fi
  LI_UI_INIT=1
  LI_USE_COLOR=0
  if [[ -z "${NO_COLOR:-}" && -t 1 ]]; then
    LI_USE_COLOR=1
  fi
  if [[ "${CI:-}" == "true" && -z "${LI_FORCE_COLOR:-}" ]]; then
    LI_USE_COLOR=0
  fi

  LI_RESET=$'\033[0m'
  LI_DIM=$'\033[2m'
  LI_BOLD=$'\033[1m'
  LI_CYAN=$'\033[38;2;61;214;255m'
  LI_VIOLET=$'\033[38;2;124;92;255m'
  LI_MINT=$'\033[38;2;46;230;168m'
  LI_AMBER=$'\033[38;2;255;179;71m'
  LI_ROSE=$'\033[38;2;255;92;122m'
  LI_ICE=$'\033[38;2;232;238;247m'
  LI_MUTED=$'\033[38;2;139;156;179m'

  if [[ "$LI_USE_COLOR" -eq 0 ]]; then
    LI_RESET="" LI_DIM="" LI_BOLD="" LI_CYAN="" LI_VIOLET="" LI_MINT="" LI_AMBER=""
    LI_ROSE="" LI_ICE="" LI_MUTED=""
  fi
}
_li_ui_init

li_phase() {
  printf '\n%s▸ %s%s %s\n' "$LI_CYAN" "$LI_BOLD" "$1" "$LI_RESET"
}

li_ok() {
  printf '  %s◆ %s%s\n' "$LI_MINT" "$1" "$LI_RESET"
}

li_warn() {
  printf '  %s△ %s%s\n' "$LI_AMBER" "$1" "$LI_RESET"
}

li_fail() {
  printf '  %s✕ %s%s\n' "$LI_ROSE" "$1" "$LI_RESET" >&2
}

li_gate_ok() {
  printf '\n%s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n' "$LI_DIM" "$LI_RESET"
  printf '%s  GATE CLEAR  %s %s\n' "$LI_MINT" "$1" "$LI_RESET"
  printf '%s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n\n' "$LI_DIM" "$LI_RESET"
}

li_gate_fail() {
  printf '\n%s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n' "$LI_DIM" "$LI_RESET"
  printf '%s  GATE HOLD  %s %s\n' "$LI_ROSE" "$1" "$LI_RESET" >&2
  printf '%s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n\n' "$LI_DIM" "$LI_RESET" >&2
}

li_banner() {
  printf '%s%s' "$LI_DIM"
  cat <<'EOF'
    ╭──────────────────────────────────────────╮
    │  ░▒▓  Li  ·  prove · write · run fast  ▓▒░  │
    ╰──────────────────────────────────────────╯
EOF
  printf '%s\n' "$LI_RESET"
}

li_test_pass() {
  # Keep "PASS " prefix for harness parsers (plot_suites.py).
  printf '%sPASS%s %s\n' "$LI_MINT" "$LI_RESET" "$*"
}

li_test_fail() {
  printf '%sFAIL%s %s\n' "$LI_ROSE" "$LI_RESET" "$*" >&2
}

li_test_skip() {
  printf '%sSKIP%s %s\n' "$LI_MUTED" "$LI_RESET" "$*"
}

li_tests_footer() {
  local pass=$1 fail=$2 skip=$3
  printf '\n%s── li-tests ─────────────────────────────%s\n' "$LI_DIM" "$LI_RESET"
  if [[ "$fail" -eq 0 ]]; then
    printf '  %s◆ %s%d passed%s' "$LI_MINT" "$LI_BOLD" "$pass" "$LI_RESET"
    [[ "$skip" -gt 0 ]] && printf '  %s· %d skipped%s' "$LI_MUTED" "$skip" "$LI_RESET"
    printf '\n'
  else
    printf '  %s✕ %d failed%s  %s· %d passed%s\n' "$LI_ROSE" "$fail" "$LI_RESET" "$LI_MUTED" "$pass" "$LI_RESET" >&2
  fi
}
