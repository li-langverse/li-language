#!/usr/bin/env python3
"""Lemma rebuild pipeline — see data/proof-db/README.md."""
from __future__ import annotations
import argparse, json, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
try:
    import tomllib
except ImportError:
    import tomli as tomllib
ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "docs/verification/proof-database/schema.toml"
JSON_OUT = ROOT / "data/proof-db/latest-report.json"
MD_OUT = ROOT / "data/proof-db/latest-report.md"
AUTOVC = ROOT / "build/generated/AutoVC.lean"
RESOLVE = ROOT / "scripts/resolve-lic.sh"
CHECK = ROOT / "scripts/check-autovc-open-goals.sh"

def schema():
    if SCHEMA.is_file():
        return tomllib.loads(SCHEMA.read_text())
    return {"schema": {"entries_root": "docs/verification/proof-database/entries"}}

def roots(s):
    out = []
    for k in ("entries_root", "corpus_root"):
        r = s.get("schema", {}).get(k)
        if r and (ROOT / r).is_dir():
            out.append(ROOT / r)
    return out or [ROOT / "docs/verification/proof-database/entries"]

def parse(path):
    d = tomllib.loads(path.read_text())
    if "entry" in d and isinstance(d["entry"], list):
        return [e for e in d["entry"] if isinstance(e, dict)]
    return [d] if d.get("id") else []

def all_rows():
    for root in roots(schema()):
        for p in sorted(root.rglob("*.toml")):
            if p.name == "schema.toml":
                continue
            for r in parse(p):
                yield r

def head():
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    except Exception:
        return "unknown"

def lic():
    if not RESOLVE.is_file():
        return None
    try:
        p = subprocess.check_output(["bash", str(RESOLVE)], cwd=ROOT, text=True).strip()
        return p if Path(p).is_file() else None
    except Exception:
        return None

def open_goals():
    if not AUTOVC.is_file():
        return -1, []
    t = AUTOVC.read_text()
    names = [ln.split()[1].split("(")[0] for ln in t.splitlines() if ln.strip().startswith("def vc_") and (": Prop :=" in ln or "decreases" in ln)]
    open_n = [n for n in names if f"theorem {n}_proved" not in t]
    if CHECK.is_file() and subprocess.run(["bash", str(CHECK), str(AUTOVC)], cwd=ROOT).returncode == 0:
        return 0, []
    return len(open_n), open_n

def build(lic_bin, spec):
    if AUTOVC.is_file():
        AUTOVC.unlink()
    r = subprocess.run([lic_bin, "build", str(spec), "-o", "/dev/null"], cwd=ROOT, capture_output=True, text=True)
    log = (r.stdout or "") + (r.stderr or "")
    return r.returncode == 0, "\n".join(log.strip().splitlines()[-6:])

def note(cat, reb, n, ok):
    if cat == "axiomatic":
        return "Axiomatic — not user-code wrongness."
    if not ok:
        return "Build failed — investigate VC/specimen."
    if cat == "proved" and reb != "proved":
        return "Catalog proved but rebuild not closed."
    if n > 0:
        return f"{n} open Prop goal(s)."
    return ""

def rebuild(row, lic_bin, skip):
    eid, cat = row.get("id"), row.get("proof_status", "open")
    spec = (row.get("li_specimen") or "").strip()
    out = {"id": eid, "kind": row.get("kind"), "proof_status_catalog": cat, "li_specimen": spec or None, "gap_id": row.get("gap_id")}
    if cat == "axiomatic":
        out.update(status="axiomatic", open_goals=None, discrepancy_notes=note(cat, "axiomatic", 0, True))
        return out
    if skip or not lic_bin:
        out.update(status="skipped", open_goals=None, discrepancy_notes="lic build skipped.")
        return out
    if not spec:
        out.update(status="skipped", open_goals=None, discrepancy_notes="No li_specimen.")
        return out
    ok, ex = build(lic_bin, ROOT / spec)
    out["build_excerpt"] = ex
    if not ok:
        out.update(status="build_failed", open_goals=None, discrepancy_notes=note(cat, "build_failed", 0, False))
        return out
    n, names = open_goals()
    reb = "proved" if n == 0 else "open"
    out.update(status=reb, open_goals=n, open_goal_names=names[:10], discrepancy_notes=note(cat, reb, max(n, 0), True))
    return out

def md(rep):
    s = rep["summary"]
    lines = ["# Proof database — lemma rebuild report", "", rep["policy"], "",
             f"- **Generated:** {rep['generated_at']}", f"- **lic commit:** `{rep['lic_commit']}`", "",
             "## Summary", "", "| proved | open | failed | skipped | axiomatic | mismatch |",
             "|--------|------|--------|---------|-----------|----------|",
             f"| {s.get('proved',0)} | {s.get('open',0)} | {s.get('build_failed',0)} | {s.get('skipped',0)} | {s.get('axiomatic',0)} | {s.get('mismatch_catalog',0)} |",
             "", "## Per entry", "", "| id | catalog | rebuild | open_goals | notes |", "|----|---------|---------|------------|-------|"]
    for r in rep["entries"]:
        og = r.get("open_goals")
        lines.append(f"| {r['id']} | {r['proof_status_catalog']} | {r['status']} | {'—' if og is None else og} | {(r.get('discrepancy_notes') or '')[:70]} |")
    lines += ["", "Failures = proof gaps, not user-code wrongness.", ""]
    return "\n".join(lines)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--validate-only", action="store_true")
    a = ap.parse_args()
    loaded = list(all_rows())
    if a.validate_only:
        print(f"rebuild_lemmas: OK ({len(loaded)} entries)" if loaded else "no entries", file=sys.stderr)
        return 0 if loaded else 1
    lb = None if a.skip_build else lic()
    entries = [rebuild(r, lb, a.skip_build) for r in loaded if r.get("kind") in ("axiom", "lemma")]
    summary, mismatch = {}, 0
    for e in entries:
        summary[e["status"]] = summary.get(e["status"], 0) + 1
        if e["proof_status_catalog"] == "proved" and e["status"] not in ("proved", "skipped", "axiomatic"):
            mismatch += 1
    summary["mismatch_catalog"] = mismatch
    rep = {"version": 1, "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
           "lic_commit": head(), "lic_binary": lb,
           "policy": "> **Policy:** failure/open = gap to investigate — not Li wrongness.",
           "entry_count": len(entries), "summary": summary, "entries": entries}
    JSON_OUT.parent.mkdir(parents=True, exist_ok=True)
    JSON_OUT.write_text(json.dumps(rep, indent=2) + "\n")
    MD_OUT.write_text(md(rep))
    print(f"rebuild_lemmas: wrote {JSON_OUT.relative_to(ROOT)} ({len(entries)} entries)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
