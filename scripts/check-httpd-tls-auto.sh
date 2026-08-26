#!/usr/bin/env bash
# M1.5 TLS auto: validate-config, setup-tls, flatten gates (Python).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"

echo "== tls good configs =="
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/tls_self_signed_dev.toml"
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/tls_lets_encrypt_staging.toml"

echo "== tls reject configs =="
for rej in \
  "$ROOT/li-tests/config_desugar/reject/tls_public_self_signed.toml" \
  "$ROOT/li-tests/config_desugar/reject/tls_public_no_tls.toml" \
  "$ROOT/li-tests/config_desugar/reject/tls_le_missing_email.toml"; do
  name="$(basename "$rej")"
  if python3 "$ROOT/scripts/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

echo "== setup-tls self_signed =="
python3 "$ROOT/scripts/setup-tls-httpd.py" \
  "$ROOT/li-tests/config_desugar/good/tls_self_signed_dev.toml" \
  -o "$work/certs-dev"
test -f "$work/certs-dev/fullchain.pem"
test -f "$work/certs-dev/privkey.pem"

echo "== setup-tls lets_encrypt staging (dry-run) =="
python3 "$ROOT/scripts/setup-tls-httpd.py" \
  "$ROOT/li-tests/config_desugar/good/tls_lets_encrypt_staging.toml" \
  -o "$work/certs-staging" \
  --dry-run
test -f "$work/certs-staging/acme-renewal.json"
test -f "$work/certs-staging/fullchain.pem"

echo "== setup-tls renew =="
python3 "$ROOT/scripts/setup-tls-httpd.py" \
  "$ROOT/li-tests/config_desugar/good/tls_lets_encrypt_staging.toml" \
  -o "$work/certs-staging" \
  --renew --dry-run

echo "== flatten tls_lets_encrypt_staging =="
tmp="$(mktemp)"
python3 "$ROOT/scripts/flatten-httpd-config.py" \
  "$ROOT/li-tests/config_desugar/good/tls_lets_encrypt_staging.toml" -o "$tmp"
grep -q 'tls_enabled=1' "$tmp"
grep -q 'tls_mode=lets_encrypt' "$tmp"
grep -q 'tls_le_email=ops@staging.example.com' "$tmp"
grep -q 'tls_acme_reserved_path=/.well-known/acme-challenge/' "$tmp"
rm -f "$tmp"

echo "== li-tests/tls_setup smoke =="
"$ROOT/li-tests/tls_setup/run_setup_tls.sh" "$work"

echo "check-httpd-tls-auto: OK"
