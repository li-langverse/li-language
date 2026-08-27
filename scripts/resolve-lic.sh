#!/usr/bin/env bash
# Print path to built lic (handles lic vs lic.exe on Windows/Git Bash).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for candidate in "$ROOT/build/compiler/lic/lic.exe" "$ROOT/build/compiler/lic/lic"; do
  if [[ -x "$candidate" ]]; then
    echo "$candidate"
    exit 0
  fi
done
echo "resolve-lic: no executable under build/compiler/lic/" >&2
exit 1
