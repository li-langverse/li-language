import json, os, re
from pathlib import Path
ROOT = Path(os.environ["ROOT"])
THM = re.compile(r"^theorem\s+(\w+)")
SORRY = re.compile(r"\bsorry\b")
PH = re.compile(r"placeholder", re.I)
SOURCES = [
    ("docs/semantics/Core.lean", "Li"),
    ("docs/semantics/Discharge.lean", "Li.Discharge"),
    ("proof-db/lean/ProofDB.lean", "Li.ProofDB"),
    ("build/generated/AutoVC.lean", "AutoVC"),
]

def classify(name, body):
    if SORRY.search(body):
        return "open"
    if PH.search(name) or name.endswith("_stub") or "stub" in name.lower():
        return "placeholder"
    if ":= rfl" in body or ":= trivial" in body:
        return "proved"
    if re.search(r":=\s*(Int|Float|Li)\.", body):
        return "proved"
    if ":= by" in body:
        return "proved"
    return "open"

def scan(path, source):
    if not path.is_file():
        return []
    rel = str(path.relative_to(ROOT))
    lines = path.read_text(encoding="utf-8").splitlines()
    rows, i = [], 0
    while i < len(lines):
        m = THM.match(lines[i].strip())
        if not m:
            i += 1
            continue
        name = m.group(1)
        chunk = [lines[i]]
        i += 1
        while i < len(lines):
            chunk.append(lines[i])
            if THM.match(lines[i].strip()):
                break
            if ":=" in lines[i] and not lines[i].strip().startswith("--"):
                j = i + 1
                while j < len(lines) and lines[j].startswith(" "):
                    chunk.append(lines[j])
                    j += 1
                i = j
                break
            i += 1
        rows.append({
            "id": f"{rel}:{name}",
            "name": name,
            "status": classify(name, "\n".join(chunk)),
            "source": source,
            "file": rel,
        })
    return rows

all_rows = []
for rel, source in SOURCES:
    p = ROOT / rel
    if rel.endswith("AutoVC.lean") and not p.is_file():
        continue
    all_rows.extend(scan(p, source))
for row in sorted(all_rows, key=lambda r: r["id"]):
    print(json.dumps(row, sort_keys=True))
