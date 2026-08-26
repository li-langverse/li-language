#!/usr/bin/env bash
# m1-nginx-bench-parity: tier5_http agent-gateway scenarios meet or beat nginx (RPS/latency ratios).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
HARNESS="$ROOT/benchmarks/harness"
export PYTHONPATH="$HARNESS${PYTHONPATH:+:$PYTHONPATH}"
export LI_HTTPD_BIN="$HTTPD"

if [[ ! -x "$HTTPD" ]]; then
  echo "check-tier5-nginx-bench-parity: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

# Prefer /usr/sbin/nginx and a local wrk when not on PATH.
export PATH="/usr/sbin:/usr/local/bin:${PATH:-}"

chmod +x "$HARNESS/bench_http.py" "$HARNESS/verify_http.py" 2>/dev/null || true

echo "==> verify_http.py --profile parity (nginx + li-httpd)"
python3 "$HARNESS/verify_http.py" --profile parity

if [[ "${HTTPD_BENCH_SKIP_TIMING:-0}" == "1" ]]; then
  echo "check-tier5-nginx-bench-parity: skip timing (HTTPD_BENCH_SKIP_TIMING=1)"
  echo "check-tier5-nginx-bench-parity: OK"
  exit 0
fi

if ! command -v wrk >/dev/null 2>&1; then
  echo "check-tier5-nginx-bench-parity: wrk missing — verify-only OK (set HTTPD_BENCH_SKIP_TIMING=1 to silence)" >&2
  echo "check-tier5-nginx-bench-parity: OK"
  exit 0
fi

NGINX_BIN="$(PYTHONPATH="$HARNESS" python3 -c 'from http_bench_servers import resolve_nginx_binary; print(resolve_nginx_binary() or "")')"
if [[ -z "$NGINX_BIN" ]]; then
  echo "check-tier5-nginx-bench-parity: nginx missing — verify-only OK" >&2
  echo "check-tier5-nginx-bench-parity: OK"
  exit 0
fi

# Short load for CI gate; override with HTTPD_BENCH_DURATION_SEC.
DUR="${HTTPD_BENCH_DURATION_SEC:-8}"
echo "==> bench_http.py --profile parity --check-parity (duration ${DUR}s)"
python3 "$HARNESS/bench_http.py" --profile parity --check-parity \
  --set "load.duration_sec=${DUR}" \
  --out "$ROOT/benchmarks/results/tier5_http_parity.csv"

echo "check-tier5-nginx-bench-parity: OK"
