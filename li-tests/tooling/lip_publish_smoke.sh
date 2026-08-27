#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
"$ROOT/scripts/lip" publish --dry-run
"$ROOT/scripts/lip" publish 2>&1 | grep -q 'lip publish: ok'
echo "lip_publish_smoke: ok"
