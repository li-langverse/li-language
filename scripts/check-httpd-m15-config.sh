#!/usr/bin/env bash
# M1.5 agent gateway config oracle (Python — no lic build required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"

echo "== m15 good configs =="
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/agent_m15.toml"

echo "== m15 reject configs =="
for rej in \
  "$ROOT/li-tests/config_desugar/reject/m15_missing_stream_limits.toml" \
  "$ROOT/li-tests/config_desugar/reject/m15_inference_no_traceparent.toml"; do
  name="$(basename "$rej")"
  if python3 "$ROOT/scripts/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

echo "== flatten agent_m15 =="
tmp="$(mktemp)"
python3 "$ROOT/scripts/flatten-httpd-config.py" \
  "$ROOT/li-tests/config_desugar/good/agent_m15.toml" -o "$tmp"
grep -q 'stream_idle_timeout_sec=120' "$tmp"
grep -q 'model_match=gpt-4|11435' "$tmp"
grep -q 'route_require=POST|/v1/chat|traceparent' "$tmp"
rm -f "$tmp"

echo "check-httpd-m15-config: OK"
