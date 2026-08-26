#!/usr/bin/env bash
# M3 optional: L4 stream + token-budget hooks (RFC + validate/flatten oracle).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"

echo "== m3 good config =="
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/m3_optional.toml"

echo "== m3 reject configs =="
for rej in \
  "$ROOT/li-tests/config_desugar/reject/m3_l4_no_upstream.toml" \
  "$ROOT/li-tests/config_desugar/reject/m3_l4_private_upstream.toml" \
  "$ROOT/li-tests/config_desugar/reject/m3_token_budget_bad_header.toml" \
  "$ROOT/li-tests/config_desugar/reject/m3_token_cap_excess.toml"; do
  name="$(basename "$rej")"
  if python3 "$ROOT/scripts/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

echo "== flatten m3_optional =="
python3 "$ROOT/scripts/flatten-httpd-config.py" \
  "$ROOT/li-tests/config_desugar/good/m3_optional.toml" -o "$tmp"
grep -q 'm3_enabled=1' "$tmp"
grep -q 'm3_l4_enabled=1' "$tmp"
grep -q 'm3_l4_listen_port=9443' "$tmp"
grep -q 'm3_l4_upstream_port=11435' "$tmp"
grep -q 'm3_l4_max_connections=512' "$tmp"
grep -q 'm3_token_budget_enabled=1' "$tmp"
grep -q 'm3_token_budget_header=x-token-budget' "$tmp"
grep -q 'm3_token_budget_max=500000' "$tmp"

echo "check-httpd-m3-config: OK"
