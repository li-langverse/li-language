#!/usr/bin/env bash
# Build and install the `lic` compiler in one command, and wire its bin dir
# into your shell profile so `lic` is on PATH in new shells.
#
# Usage:
#   ./scripts/install.sh                            # -> ~/.local/bin/lic, PATH wired
#   LI_INSTALL_PREFIX=/usr/local ./scripts/install.sh
#   LI_INSTALL_BIN=/custom/bin ./scripts/install.sh
#   LI_NO_PROFILE=1 ./scripts/install.sh            # skip shell-profile PATH wiring
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

PREFIX="${LI_INSTALL_PREFIX:-$HOME/.local}"
BINDIR="${LI_INSTALL_BIN:-$PREFIX/bin}"
LIC_SRC="$ROOT/build/compiler/lic/lic"

sh_snippet() {
  local dir="$1"
  printf '\n# >>> lic install >>>\nexport PATH="%s:$PATH"\n# <<< lic install <<<\n' "$dir"
}

fish_snippet() {
  local dir="$1"
  printf '\n# >>> lic install >>>\nset -gx PATH %s $PATH\n# <<< lic install <<<\n' "$dir"
}

# Append a snippet to a file once, keyed by the `>>> lic install >>>` marker,
# so re-running install.sh is idempotent.
append_snippet() {
  local file="$1" snippet="$2"
  mkdir -p "$(dirname "$file")"
  if [[ -f "$file" ]] && grep -qF '# >>> lic install >>>' "$file"; then
    echo "PATH already wired in $file"
    return 0
  fi
  printf '%s' "$snippet" >> "$file"
  echo "wired PATH into $file"
}

wire_shell_paths() {
  local shell_name
  shell_name="$(basename "${SHELL:-/bin/sh}")"

  case "$shell_name" in
    bash)
      # Login shells read ~/.bash_profile (or ~/.bash_login) first; interactive
      # non-login shells read ~/.bashrc. Cover both when present.
      if [[ -f "$HOME/.bash_profile" ]]; then
        append_snippet "$HOME/.bash_profile" "$(sh_snippet "$BINDIR")"
      elif [[ -f "$HOME/.bash_login" ]]; then
        append_snippet "$HOME/.bash_login" "$(sh_snippet "$BINDIR")"
      fi
      append_snippet "$HOME/.bashrc" "$(sh_snippet "$BINDIR")"
      ;;
    zsh)
      append_snippet "$HOME/.zshrc" "$(sh_snippet "$BINDIR")"
      ;;
    fish)
      append_snippet "$HOME/.config/fish/config.fish" "$(fish_snippet "$BINDIR")"
      ;;
    *)
      append_snippet "$HOME/.profile" "$(sh_snippet "$BINDIR")"
      ;;
  esac
}

# 1. Build (auto-detects LLVM 22 / compilers via scripts/llvm-env.sh).
"$ROOT/scripts/build.sh"

# 2. Install the single self-contained binary.
mkdir -p "$BINDIR"
install -m 0755 "$LIC_SRC" "$BINDIR/lic"

echo "installed lic -> $BINDIR/lic"

# 3. Wire the install dir into the user's shell profile so PATH is automatic.
if [[ "${LI_NO_PROFILE:-0}" == "1" ]]; then
  echo "skipping shell-profile PATH wiring (LI_NO_PROFILE=1)"
else
  wire_shell_paths
  echo "note: PATH changes take effect in a new shell."
fi

# 4. Smoke-check the installed binary.
"$BINDIR/lic" --version
