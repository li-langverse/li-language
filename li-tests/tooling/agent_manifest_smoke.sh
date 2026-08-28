#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
chmod +x "$ROOT/scripts/export-li-tests-agent-slice.sh"
"$ROOT/scripts/export-li-tests-agent-slice.sh"

OUT="$ROOT/li-tests/agent-manifest.json"
grep -q '"schema": "li-tests-agent-manifest-v1"' "$OUT" || {
  echo "agent_manifest_smoke: bad schema" >&2
  exit 1
}
python3 - "$OUT" <<'PY'
import json, sys
doc = json.load(open(sys.argv[1]))
assert doc["count"] == len(doc["tests"])
assert doc["count"] > 100
suites = doc["suites"]
assert isinstance(suites, list) and len(suites) >= 5
assert sum(s["count"] for s in suites) == doc["count"]
assert all("name" in s and "count" in s for s in suites)
PY
echo "agent_manifest_smoke: ok"
