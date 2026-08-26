#!/usr/bin/env python3
import json, os, re, sys
from datetime import datetime, timezone
from pathlib import Path

root = Path(os.environ["ROOT"])
catalog = json.loads(Path(os.environ["CATALOG"]).read_text())
toml = Path(os.environ["ENTRIES"]).read_text()
out = Path(os.environ["OUT"])
ids = [e["id"] for e in catalog["entries"]]
reg = [i for i in ids if re.search("catalog_id=" + re.escape(i), toml)]
rows = []
for block in re.split(r"\n\[\[entry\]\]", "\n" + toml)[1:]:
    m = re.search(r'^id = "([^"]+)"', block, re.M)
    if not m or not m.group(1).startswith("M-LM-LMATH-"):
        continue
    eid = m.group(1)
    n = re.search(r'^notes = "([^"]*)"', block, re.M)
    cid = "—"
    if n and "catalog_id=" in n.group(1):
        cid = n.group(1).split("catalog_id=", 1)[1].split(";", 1)[0]
    st = re.search(r'^proof_status = "([^"]+)"', block, re.M)
    rows.append((cid, eid, st.group(1) if st else ""))
reb = {}
rp = root / "data/proof-db/latest-report.json"
if rp.is_file():
    reb = {e["id"]: e for e in json.loads(rp.read_text()).get("entries", [])}
lines = [
    "# Proof database — gap2 catalog slice",
    "",
    "- **Generated:** " + datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    f"- **Registered:** {len(reg)}/{len(ids)}",
    "",
    "> **Policy:** open / build_failed = proof gap — not Li user-code wrongness.",
    "",
    "| catalog_id | m_lm_id | catalog | proof_status | rebuild |",
    "|------------|---------|---------|--------------|---------|",
]
for cid, eid, st in rows:
    ce = next((e for e in catalog["entries"] if e["id"] == cid), {})
    lines.append(f"| {cid} | {eid} | {ce.get('status', '—')} | {st} | {reb.get(eid, {}).get('status', '—')} |")
if [i for i in ids if i not in reg]:
    sys.exit(1)
lines += ["", "- All L-MATH catalog targets registered."]
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines) + "\n")
print("proof-db-gap2-report: wrote", out.relative_to(root), file=sys.stderr)
