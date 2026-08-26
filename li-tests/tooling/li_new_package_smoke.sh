#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

chmod +x "$ROOT/scripts/li-new-package"
"$ROOT/scripts/li-new-package" "li-smoke-test" --kind library --out "$TMP/li-smoke-test"

for f in li.toml README.md PUBLISH.md CHANGELOG.md SECURITY.md src/lib.li \
  docs/traceability.md li-tests/smoke/builds.li; do
  [[ -f "$TMP/li-smoke-test/$f" ]] || { echo "missing $f"; exit 1; }
done

grep -q 'li-langverse' "$TMP/li-smoke-test/li.toml" || { echo "li.toml missing org url"; exit 1; }

export LIC="$("$ROOT/scripts/resolve-lic.sh")"
if [[ -x "$LIC" ]]; then
  "$LIC" check "$TMP/li-smoke-test/li-tests/smoke/builds.li" || exit 1
else
  echo "skip lic check (not built)"
fi

echo "li_new_package_smoke: ok"
