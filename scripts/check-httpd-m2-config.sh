#!/usr/bin/env bash
# M2 scale: TLS1.3 terminate, HTTP/2, WebSocket, circuit breaker, queue 429, webhook allowlist.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"

echo "== m2 good config =="
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/m2_agent_scale.toml"

echo "== m2 reject configs =="
for rej in \
  "$ROOT/li-tests/config_desugar/reject/m2_http2_no_tls.toml" \
  "$ROOT/li-tests/config_desugar/reject/m2_webhook_private_ip.toml" \
  "$ROOT/li-tests/config_desugar/reject/m2_queue_depth_excess.toml"; do
  name="$(basename "$rej")"
  if python3 "$ROOT/scripts/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

echo "== flatten m2_agent_scale =="
python3 "$ROOT/scripts/flatten-httpd-config.py" \
  "$ROOT/li-tests/config_desugar/good/m2_agent_scale.toml" -o "$tmp"
grep -q 'm2_enabled=1' "$tmp"
grep -q 'm2_tls_terminate=1' "$tmp"
grep -q 'm2_http2_enabled=1' "$tmp"
grep -q 'm2_queue_max_depth=50' "$tmp"
grep -q 'm2_cb_error_threshold=5' "$tmp"
grep -q 'm2_webhook_allow=https://hooks.example.com/v1/callback' "$tmp"
grep -q 'route_require=GET|/ws/tools|websocket' "$tmp"

echo "check-httpd-m2-config: OK"
