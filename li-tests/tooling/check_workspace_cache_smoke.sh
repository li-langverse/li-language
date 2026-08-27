#!/usr/bin/env bash
# Smoke: lic check --workspace with per-member subprocesses and diagnostic JSON LRU cache.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="${LI_REPO_ROOT:-$ROOT}"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

fail() {
  echo "check_workspace_cache_smoke: $*" >&2
  exit 1
}

TMP="$(mktemp -d "${TMPDIR:-/tmp}/lic-ws-check.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/packages/pkg-a/src"
cp "$ROOT/li-tests/typecheck/fib.li" "$TMP/packages/pkg-a/src/lib.li"
cat >"$TMP/packages/li.toml" <<'EOF'
[workspace]
members = ["pkg-a"]
resolver = "path"
EOF

CACHE="$TMP/check-cache"
WS="$TMP/packages/li.toml"

if "$LIC" check --workspace="$WS" --jobs=1 --cache-dir="$CACHE" \
  "$TMP/packages/pkg-a/src/lib.li" 2>/dev/null; then
  fail "workspace mode must not accept a file path"
fi

out="$("$LIC" check --workspace="$WS" --jobs=1 --cache-dir="$CACHE" 2>&1)" || fail "workspace check failed: $out"
echo "$out" | grep -q 'lic check --workspace: ok' || fail "missing ok summary: $out"

count="$(find "$CACHE" -maxdepth 1 -name '*.json' 2>/dev/null | wc -l | tr -d ' ')"
[[ "$count" -ge 1 ]] || fail "expected cache entry after first run (dir=$CACHE)"

out2="$("$LIC" check --format=json --cache-dir="$CACHE" "$TMP/packages/pkg-a/src/lib.li" 2>&1)" || true
echo "$out2" | grep -q '"ok":true' || fail "cached single-file check: ${out2:0:400}"

nocache="$("$LIC" check --workspace="$WS" --jobs=1 --no-cache 2>&1)" || fail "no-cache workspace failed"
echo "$nocache" | grep -q 'lic check --workspace: ok' || fail "no-cache summary missing"

echo "check_workspace_cache_smoke: ok"
