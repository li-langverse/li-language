#!/usr/bin/env bash
# Validate security/historic-bugs.toml ↔ li-tests (all OS, no network).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REGISTRY="$ROOT/security/historic-bugs.toml"

if [[ ! -f "$REGISTRY" ]]; then
  echo "FAIL missing $REGISTRY"
  exit 1
fi

python3 - "$REGISTRY" "$ROOT" <<'PY'
import re
import sys
from pathlib import Path

registry_path, root = Path(sys.argv[1]), Path(sys.argv[2])
text = registry_path.read_text(encoding="utf-8")
fail = 0

blocks = re.split(r"\n\[\[bug\]\]", text)
bugs = []
for block in blocks[1:]:
    bid = re.search(r'^id = "([^"]+)"', block, re.M)
    if not bid:
        continue
    bugs.append((bid.group(1), block))

if len(bugs) < 10:
    print(f"FAIL historic registry has {len(bugs)} bugs (need >= 10)")
    fail += 1

enforced = 0
for bug_id, block in bugs:
    status_m = re.search(r'^mitigation_status = "([^"]+)"', block, re.M)
    test_m = re.search(r'^li_test = "([^"]+)"', block, re.M)
    test_m_null = re.search(r'^li_test = null', block, re.M)
    status = status_m.group(1) if status_m else ""
    if status == "enforced":
        enforced += 1
        if test_m_null or not test_m:
            print(f"FAIL {bug_id}: enforced but no li_test")
            fail += 1
            continue
        path = root / test_m.group(1)
        if not path.exists():
            print(f"FAIL {bug_id}: missing {test_m.group(1)}")
            fail += 1

if enforced < 5:
    print(f"FAIL only {enforced} enforced historic bugs (need >= 5)")
    fail += 1

if fail:
    sys.exit(1)
print(f"check-historic-bugs: ok ({len(bugs)} registered, {enforced} enforced with tests)")
PY
