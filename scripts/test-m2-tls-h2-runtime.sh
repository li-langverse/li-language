#!/usr/bin/env bash
# m2-tls-h2-runtime: live li-httpd TLS 1.3 + ALPN h2 (curl --http2) and https HTTP/1.1.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${TLS_H2_CFG:-$ROOT/packages/li-net-httpd/examples/tls_h2.toml}"
CONF="/tmp/httpd-tls-h2.conf"
PUBLIC="/tmp/httpd-tls-h2-public"
CERT_DIR="/tmp/httpd-tls-h2-certs"
PORT=18443

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m2-tls-h2-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC" "$CERT_DIR"
echo ok > "$PUBLIC/health"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/setup-tls-httpd.py" "$CFG" --cert-dir "$CERT_DIR"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
# Point flattened cert_dir at generated material
sed -i "s|^tls_cert_dir=.*|tls_cert_dir=${CERT_DIR}|" "$CONF"
grep -q '^m2_tls_terminate=1' "$CONF"
grep -q '^m2_http2_enabled=1' "$CONF"
grep -q '^tls_enabled=1' "$CONF"

fuser -k "${PORT}/tcp" 2>/dev/null || true
sleep 0.3

LI_HTTPD_WORKERS=1 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 1.2

fail=0

# TLS 1.3 + ALPN (openssl s_client)
alpn=$(echo | timeout 5 openssl s_client -connect "127.0.0.1:${PORT}" -servername localhost -alpn 'h2,http/1.1' -tls1_3 2>/dev/null \
  | awk '/ALPN protocol:/{print $3; exit}')
if [[ "$alpn" != "h2" && "$alpn" != "http/1.1" ]]; then
  echo "test-m2-tls-h2-runtime: FAIL ALPN expected h2 or http/1.1 got '${alpn:-none}'" >&2
  fail=1
fi

# HTTP/2 over TLS
h2_hdr=$(curl -k -s -m 5 --http2 "https://127.0.0.1:${PORT}/health" -D /tmp/httpd-m2-h2.hdr -o /tmp/httpd-m2-h2.out 2>/dev/null || true)
h2_body=$(cat /tmp/httpd-m2-h2.out 2>/dev/null || true)
if ! grep -q 'HTTP/2 200' /tmp/httpd-m2-h2.hdr 2>/dev/null; then
  echo "test-m2-tls-h2-runtime: FAIL h2 expected HTTP/2 200 (hdr: $(head -1 /tmp/httpd-m2-h2.hdr 2>/dev/null || echo none))" >&2
  fail=1
fi
if [[ "$h2_body" != *"ok"* ]]; then
  echo "test-m2-tls-h2-runtime: FAIL h2 body missing ok (got: ${h2_body:-empty})" >&2
  fail=1
fi

# HTTP/1.1 over TLS (force no h2)
h11_code=$(curl -k -s -m 5 --http1.1 "https://127.0.0.1:${PORT}/health" -o /tmp/httpd-m2-h11.out -w "%{http_code}" 2>/dev/null || echo "000")
if [[ "$h11_code" != "200" && "$h11_code" != "404" ]]; then
  echo "test-m2-tls-h2-runtime: FAIL https/1.1 expected 200/404 got $h11_code" >&2
  fail=1
fi

kill "$FE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m2-tls-h2-runtime: ok (TLS1.3 ALPN, HTTP/2 GET /health)"
