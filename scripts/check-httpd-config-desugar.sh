#!/usr/bin/env bash
# Golden: every li-tests/config_desugar/good/*.toml matches *.explained.golden (C/Python parity).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GOOD="$ROOT/li-tests/config_desugar/good"
chmod +x "$ROOT/scripts/check-httpd-explain-config.sh" "$ROOT/scripts/li-httpd-explain-config.sh"

for cfg in "$GOOD"/*.toml; do
  base="$(basename "$cfg" .toml)"
  golden="$GOOD/${base}.explained.golden"
  if [[ ! -f "$golden" ]]; then
    echo "check-httpd-config-desugar: missing golden for $base" >&2
    exit 1
  fi
  echo "== $base =="
  "$ROOT/scripts/check-httpd-explain-config.sh" "$cfg" "$golden"
done
echo "check-httpd-config-desugar: ok"
