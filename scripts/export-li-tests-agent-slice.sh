#!/usr/bin/env bash
# Vision-LLM: compact agent-readable slice of li-tests/manifest.toml (grep-friendly JSON).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$ROOT/li-tests/manifest.toml"
OUT="$ROOT/li-tests/agent-manifest.json"

if [[ ! -f "$MANIFEST" ]]; then
  echo "export-li-tests-agent-slice: missing $MANIFEST" >&2
  exit 1
fi

python3 - "$MANIFEST" "$OUT" <<'PY'
import json
import re
import sys

manifest_path, out_path = sys.argv[1], sys.argv[2]
rows = []
suite = file = outcome = substr = None

def flush():
    global suite, file, outcome, substr
    if file and outcome:
        rows.append(
            {
                "suite": suite or "",
                "file": file,
                "outcome": outcome,
                **({"expected_substr": substr} if substr else {}),
            }
        )
    suite = file = outcome = substr = None

with open(manifest_path, encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if line == "[[tests]]":
            flush()
            continue
        m = re.match(r'^suite = "(.*)"$', line)
        if m:
            suite = m.group(1)
            continue
        m = re.match(r'^file = "(.*)"$', line)
        if m:
            file = m.group(1)
            continue
        m = re.match(r'^outcome = "(.*)"$', line)
        if m:
            outcome = m.group(1)
            continue
        m = re.match(r'^expected_substr = "(.*)"$', line)
        if m:
            substr = m.group(1)
            continue
flush()

suite_counts: dict[str, int] = {}
for row in rows:
    name = row["suite"] or "(default)"
    suite_counts[name] = suite_counts.get(name, 0) + 1
suites = [
    {"name": name, "count": count}
    for name, count in sorted(suite_counts.items(), key=lambda x: x[0])
]

doc = {
    "schema": "li-tests-agent-manifest-v1",
    "source": "li-tests/manifest.toml",
    "count": len(rows),
    "suites": suites,
    "tests": rows,
}

with open(out_path, "w", encoding="utf-8") as out:
    json.dump(doc, out, indent=2)
    out.write("\n")

print(f"export-li-tests-agent-slice: wrote {out_path} ({len(rows)} rows)")
PY
