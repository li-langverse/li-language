#!/usr/bin/env bash
# Stub: read lic JSON diagnostics and print human-oriented fix suggestions.
# Usage: lic check --format=json file.li 2>/dev/null | ./scripts/lic-fix-suggest.sh
#    or: lic diagnose file.li | ./scripts/lic-fix-suggest.sh
set -euo pipefail

if ! command -v jq >/dev/null 2>&1; then
  echo "lic-fix-suggest: jq required" >&2
  exit 1
fi

payload="$(cat)"
if [[ -z "$payload" ]]; then
  echo "lic-fix-suggest: empty stdin (pipe lic check --format=json or lic diagnose)" >&2
  exit 1
fi

ok="$(echo "$payload" | jq -r '.ok')"
if [[ "$ok" == "true" ]]; then
  echo "lic-fix-suggest: no diagnostics"
  exit 0
fi

count="$(echo "$payload" | jq '.diagnostics | length')"
echo "lic-fix-suggest: $count diagnostic(s)"

echo "$payload" | jq -r '.diagnostics[] |
  "[\(.code)] \(.file):\(.line):\(.column) — \(.message)"'

echo "$payload" | jq -r '
  .diagnostics[]
  | select(.code == "type.index")
  | "  hint: tighten index refinements or prove bounds before access at \(.file):\(.line)"'

echo "$payload" | jq -r '
  .diagnostics[]
  | select(.code == "parse.indent")
  | "  hint: fix indentation to match enclosing block at \(.file):\(.line)"'

echo "$payload" | jq -r '
  .diagnostics[]
  | select(.fix_hint != null)
  | "  structured fix: \(.fix_hint)"'

# Exit 0 so CI smoke and agents can pipe without failing the shell
exit 0
