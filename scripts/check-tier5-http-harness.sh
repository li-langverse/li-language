#!/usr/bin/env bash
# tier5_http TOML harness smoke (no nginx/wrk required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export TIER5_HTTP_STUB=1
HARNESS="$ROOT/benchmarks/harness"

echo "== tier5_http TOML merge unit checks =="
python3 - <<PY
import sys
from pathlib import Path

sys.path.insert(0, "$ROOT/benchmarks/harness")
from http_bench_toml import (
    list_scenario_names,
    merge_scenario,
    parse_set_kv,
    render_nginx_conf,
    validate_merged,
)

names = list_scenario_names(profile="ci", explicit=None)
assert "static_small" in names, names
cfg = merge_scenario("static_small", profile="ci", overrides=["load.connections=64"])
assert cfg["load"]["connections"] == 64
assert not validate_merged(cfg)
text = render_nginx_conf(cfg, port=19001)
assert "19001" in text and "worker_processes" in text
key, val = parse_set_kv("load.threads=4")
assert key == "load.threads" and val == 4
print("http_bench_toml: OK")
PY

chmod +x "$ROOT/scripts/check-tier5-http-harness.sh" 2>/dev/null || true
chmod +x "$HARNESS/bench_http.py" "$HARNESS/verify_http.py" "$HARNESS/plot_http.py" 2>/dev/null || true

echo "== bench_http.py --dry-run =="
python3 "$HARNESS/bench_http.py" --profile ci --dry-run

echo "== verify_http.py (stub) =="
python3 "$HARNESS/verify_http.py" --profile ci --stub

echo "== bench_http.py --verify-only =="
python3 "$HARNESS/bench_http.py" --profile ci --verify-only

echo "== render nginx.conf.in =="
tmp="$(mktemp)"
python3 "$HARNESS/bench_http.py" static_small --render-nginx-only "$tmp"
grep -q 'listen 127.0.0.1:' "$tmp"
rm -f "$tmp"

echo "check-tier5-http-harness: OK"
