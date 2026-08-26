#!/usr/bin/env bash
# Validate security/webserver-bugs.toml ↔ li-tests (all OS).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REGISTRY="$ROOT/security/webserver-bugs.toml"

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
    if bid:
        bugs.append((bid.group(1), block))

if len(bugs) < 12:
    print(f"FAIL webserver registry has {len(bugs)} entries (need >= 12)")
    fail += 1

enforced = partial = 0
for block in blocks[1:]:
    bid = re.search(r'^id = "([^"]+)"', block, re.M)
    if not bid:
        continue
    bug_id = bid.group(1)
    status_m = re.search(r'^mitigation_status = "([^"]+)"', block, re.M)
    test_m = re.search(r'^li_test = "([^"]+)"', block, re.M)
    test_null = re.search(r'^li_test = null', block, re.M)
    status = status_m.group(1) if status_m else ""
    if status == "enforced":
        enforced += 1
        if test_null or not test_m:
            print(f"FAIL {bug_id}: enforced but no li_test")
            fail += 1
        elif not (root / test_m.group(1)).exists():
            print(f"FAIL {bug_id}: missing {test_m.group(1)}")
            fail += 1
    elif status == "partial":
        partial += 1
        if test_m and not (root / test_m.group(1)).exists():
            print(f"FAIL {bug_id}: missing partial test {test_m.group(1)}")
            fail += 1

if enforced < 4:
    print(f"FAIL only {enforced} enforced webserver mitigations (need >= 4)")
    fail += 1

if fail:
    sys.exit(1)
print(
    f"check-webserver-bugs: ok ({len(bugs)} entries, "
    f"{enforced} enforced, {partial} partial, rest httpd_design/roadmap)"
)
PY
