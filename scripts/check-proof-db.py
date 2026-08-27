import json, os, subprocess, sys
from pathlib import Path
root = Path(os.environ["ROOT"])
baseline_path = Path(os.environ["BASELINE"])
strict = os.environ.get("STRICT", "0") == "1"
export_sh = root / "scripts/export-proof-db.sh"

def load_jsonl(path):
    rows = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        row = json.loads(line)
        st = row.get("status")
        if st not in ("proved", "placeholder", "open"):
            print("check-proof-db: bad status", file=sys.stderr)
            sys.exit(1)
        rows[row["id"]] = row
    return rows

baseline = load_jsonl(baseline_path)
proc = subprocess.run([str(export_sh)], check=True, capture_output=True, text=True, cwd=root)
live = {json.loads(line)["id"]: json.loads(line) for line in proc.stdout.splitlines() if line.strip()}
if not (root / "build/generated/AutoVC.lean").is_file():
    baseline = {k: v for k, v in baseline.items() if not k.startswith("build/generated/AutoVC.lean:")}
    live = {k: v for k, v in live.items() if not k.startswith("build/generated/AutoVC.lean:")}
drifts = []
for rid in sorted(set(baseline) | set(live)):
    if rid not in baseline:
        drifts.append(f"+ {rid}")
    elif rid not in live:
        drifts.append(f"- {rid}")
    elif baseline[rid]["status"] == "proved" and live[rid]["status"] != "proved":
        drifts.append(f"~ {rid}: {baseline[rid]['status']} -> {live[rid]['status']} (regression)")
    elif baseline[rid]["status"] != live[rid]["status"]:
        drifts.append(f"~ {rid}: {baseline[rid]['status']} -> {live[rid]['status']}")
if drifts:
    for d in drifts:
        print(f"check-proof-db: drift: {d}", file=sys.stderr)
    sys.exit(1 if strict else 0)
print(f"check-proof-db: ok ({len(live)} rows)", file=sys.stderr)
