#!/usr/bin/env bash
# Phase 8a: build smoke entrypoints for [workspace].members in packages/li.toml.
# Some members (e.g. li-demo) are mirrored app/demo packages — they do not gate lic CI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="${LI_REPO_ROOT:-$ROOT}"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
WS="${1:-$ROOT/packages/li.toml}"
if [[ ! -f "$WS" ]]; then
  echo "error: workspace file not found: $WS" >&2
  exit 1
fi
members=()
while IFS= read -r name; do
  [[ -n "$name" ]] && members+=("$name")
done < <(python3 - "$WS" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'members\s*=\s*\[(.*?)\]', text, re.S)
if not m:
    sys.exit(0)
for part in re.findall(r'"([^"]+)"', m.group(1)):
    print(part)
PY
)
if [[ ${#members[@]} -eq 0 ]]; then
  echo "workspace: no members"
  exit 0
fi
for m in "${members[@]}"; do
  case "$m" in
    li-demo)
      echo "workspace build: skip $m (demo package — own repo CI, not a lic compiler gate)"
      continue
      ;;
  esac
  entry="$ROOT/packages/$m/src/lib.li"
  smoke="$ROOT/packages/$m/li-tests/smoke/builds.li"
  # Prefer smoke: package libs may use extern stubs not yet proof-complete (8a).
  if [[ -f "$smoke" ]]; then
    echo "workspace build: $m (smoke)"
    "$LIC" build "$smoke" -o /dev/null
  elif [[ -f "$entry" ]]; then
    echo "workspace build: $m (src/lib.li)"
    "$LIC" build "$entry" -o /dev/null
  else
    echo "workspace build: skip $m (no entrypoint)" >&2
  fi
done
echo "lic-workspace-build: ok (${#members[@]} members)"
