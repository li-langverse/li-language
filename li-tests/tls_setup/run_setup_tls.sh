#!/usr/bin/env bash
# Staging TLS setup smoke (no live Let's Encrypt — uses setup-tls --dry-run).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${1:-}"
if [[ -z "$WORK" ]]; then
  WORK="$(mktemp -d)"
  trap 'rm -rf "$WORK"' EXIT
fi
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"
python3 "$ROOT/scripts/setup-tls-httpd.py" \
  "$ROOT/li-tests/config_desugar/good/tls_lets_encrypt_staging.toml" \
  -o "$WORK/certs-staging" \
  --dry-run
test -s "$WORK/certs-staging/acme-renewal.json"
echo "tls_setup: OK"
