#!/usr/bin/env bash
# Smoke: stable E#### codes appear in human and JSON diagnostics.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
FIXTURE="$ROOT/li-tests/errors/missing_requires_coded.li"

human="$("$LIC" check "$FIXTURE" 2>&1)" || true
echo "$human" | grep -qE '\[E0301\]'
echo "$human" | grep -qi 'requires'

json="$("$LIC" check --format=json "$FIXTURE" 2>/dev/null)" || true
echo "$json" | grep -q '"code":"E0301"'
echo "$json" | grep -q '"fix_hint"'

echo "error_codes_smoke: ok"
