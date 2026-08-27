#!/usr/bin/env bash
# Print CHANGELOG body for a version (stdout). Used by release.yml.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:?usage: changelog-notes.sh VERSION [CHANGELOG.md]}"
CHANGELOG="${2:-${ROOT}/CHANGELOG.md}"

python3 - "$VERSION" "$CHANGELOG" <<'PY'
import sys

version, path = sys.argv[1], sys.argv[2]
header = f"## [{version}]"
lines = open(path, encoding="utf-8").read().splitlines()
capture = False
out: list[str] = []
for line in lines:
    if line.startswith("## ["):
        if capture:
            break
        if line == header or line.startswith(header + " "):
            capture = True
            continue
    if capture:
        out.append(line)
text = "\n".join(out).strip()
if not text:
    sys.exit(1)
print(text)
PY
