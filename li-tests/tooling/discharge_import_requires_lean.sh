#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
"$LIC" build "$ROOT/li-tests/modules/use_positive.li" -o /dev/null
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
"$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"
echo "discharge_import_requires_lean: ok"
