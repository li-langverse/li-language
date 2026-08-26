#!/usr/bin/env bash
# pkg-workspace gate: httpd packages aligned via li-new-package + packages/li.toml.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/scripts/check-pkg-workspace.py"
