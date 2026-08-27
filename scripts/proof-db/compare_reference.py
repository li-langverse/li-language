#!/usr/bin/env python3
"""Compare proof-db vs Discharge/ProofDB/trusted."""
from __future__ import annotations
import argparse, json, re, sys
from dataclasses import asdict, dataclass
from datetime import date
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
KINDS = ("missing_lemma", "open_vc", "spec_drift", "trusted_axiom", "hardware_axiom")
THM, DEF, AX = re.compile(r"^theorem\s+(\w+)"), re.compile(r"^def\s+(\w+)"), re.compile(r"^axiom\s+(\w+)")
SORRY, PH = re.compile(r"\bsorry\b"), re.compile(r"placeholder", re.I)

@dataclass(frozen=True)
class Sym:
    module: str; name: str; kind: str; status: str; file: str

@dataclass(frozen=True)
class D:
    id: str; kind: str; gap_id: str|None; backlog_id: str|None; summary: str
    proof_db_ref: str|None; reference_ref: str|None; evidence: list[str]; resolution: str

def st(n,b):
    if PH.search(n) or re.search(r":=\s*trivial\b",b): return "placeholder"
    if SORRY.search(b): return "sorry"
    if ":= rfl" in b or ":= by" in b: return "proved"
    return "open"

def scan(p,ns):
    if not p.is_file(): return {}
    lines,out,i=p.read_text().splitlines(),{},0
    while i<len(lines):
        s=lines[i].strip(); m=THM.match(s) or DEF.match(s) or AX.match(s)
        if not m: i+=1; continue
        name=m.group(1); kind="theorem" if THM.match(s) else "def" if DEF.match(s) else "axiom"
        ch=[lines[i]]; i+=1
        while i<len(lines) and not (THM.match(lines[i].strip()) or DEF.match(lines[i].strip()) or AX.match(lines[i].strip())):
            ch.append(lines[i]);
            if ":=" in lines[i]: break
            i+=1
        sym=Sym(ns,name,kind,"axiom" if kind=="axiom" else st(name,"\n".join(ch)),str(p.relative_to(ROOT)))
        out[f"{ns}.{name}"]=out[name]=sym
    return out

def res(ref,*tb):
    if not ref: return None
    t=ref.split(".")[-1]
    for x in tb:
        for k in (ref,t):
            if k in x: return x[k]
    return None

def seed(p):
    return [D(**r) for r in json.loads(p.read_text()).get("discrepancies",[])] if p.is_file() else []

def mrg(*g):
    d={}
    for x in g:
        for y in x: d[y.id]=y
    return sorted(d.values(),key=lambda z:z.id)

def lemmas():
    rows=[]
    d=ROOT/"proof-db/lemmas"
    if not d.is_dir(): return rows
    for p in d.glob("*.toml"):
        rid=lean=status=gap=back=None
        for ln in p.read_text().splitlines():
            if ln.startswith("id = "): rid=ln.split("=",1)[1].strip().strip(chr(34))
            if ln.startswith("lean_thm = "): lean=ln.split("=",1)[1].strip().strip(chr(34))
            if ln.startswith("proof_status = "): status=ln.split("=",1)[1].strip().strip(chr(34))
            if ln.startswith("gap_id = "): gap=ln.split("=",1)[1].strip().strip(chr(34))
            if ln.startswith("backlog_id = "): back=ln.split("=",1)[1].strip().strip(chr(34))
        if rid: rows.append({"id":rid,"lean_theorem":lean,"status":"proved" if status=="proved" else status,"gap":gap,"backlog_id":back,"file":str(p.relative_to(ROOT))})
    return rows

def main():
    a=argparse.ArgumentParser(); a.add_argument("--write",action="store_true"); a.add_argument("--json",action="store_true"); a.add_argument("--min-count",type=int,default=5); o=a.parse_args()
    dis=scan(ROOT/"docs/semantics/Discharge.lean","Li.Discharge")
    pdb=scan(ROOT/"proof-db/lean/ProofDB.lean","Li.ProofDB") if (ROOT/"proof-db/lean/ProofDB.lean").is_file() else {}
    tr=scan(ROOT/"docs/semantics/trusted.lean","Li.Trusted")
    ent=[]
    if (ROOT/"proof-db/index.json").is_file(): ent.extend(json.loads((ROOT/"proof-db/index.json").read_text()).get("entries",[]))
    ent.extend(lemmas())
    rows=mrg(seed(ROOT/"proof-db/compiler/discrepancies-seed.json"))
    for e in ent:
        ref=e.get("lean_theorem") or e.get("discharge_link"); sym=res(ref,dis,pdb)
        if ref and not sym: rows=mrg(rows,[D(f"disc-{e['id']}-missing-lean","missing_lemma",e.get("gap"),e.get("backlog_id"),f"`{ref}` missing",e.get("file","index"),"Discharge.lean",[ref],"open")])
        if sym and e.get("status")=="proved" and sym.status in ("placeholder","sorry","open"):
            rows=mrg(rows,[D(f"disc-{e['id']}-open-vc","open_vc",e.get("gap"),e.get("backlog_id"),f"{e['id']} proved vs {sym.status}",e.get("file","index"),f"{sym.file}:{sym.name}",[ref or sym.name],"intentional")])
    p=ROOT/"li-tests/contracts_verify/sqrt_open_bound.li"
    if p.is_file() and dis.get("sqrt_open_bound_placeholder"):
        h="\n".join(p.read_text().splitlines()[:5])
        if "sqrt_open_bound_spec" in h:
            ph=dis["sqrt_open_bound_placeholder"]
            rows=mrg(rows,[D("disc-sqrt-open-bound-comment-drift","spec_drift","G-vc","P-float","sqrt cites spec; Discharge placeholder",str(p.relative_to(ROOT)),f"{ph.file}:{ph.name}",[h[:80]],"open")])
    seen=set()
    for s in tr.values():
        if s.kind=="axiom" and s.name not in seen:
            seen.add(s.name); rows=mrg(rows,[D(f"disc-trusted-{s.name}","trusted_axiom","G-trust",None,f"Li.Trusted.{s.name}","trusted.lean",f"{s.file}:{s.name}",[s.name],"wontfix")])
    for e in ent:
        if e.get("status")=="sorry" or e.get("id")=="std_triangle_ineq_scalar":
            rows=mrg(rows,[D("disc-std-triangle-ineq-float","hardware_axiom","G-hw","P-float","Float triangle sorry","index","ProofDB.lean",["sorry"],"wontfix")]); break
    rows=mrg(rows,[D("disc-g-hw-float-model","hardware_axiom","G-hw","P-float","Li float model not IEEE ULP","provability-gaps.md","fp-numerical-stability.md",["G-hw"],"wontfix")])
    rep={"version":1,"generated":date.today().isoformat(),"taxonomy":list(KINDS),"count":len(rows),"discrepancies":[asdict(x) for x in rows]}
    if o.json: print(json.dumps(rep,indent=2))
    if o.write:
        od=ROOT/"proof-database"; od.mkdir(exist_ok=True)
        (od/"discrepancies.json").write_text(json.dumps(rep,indent=2)+"\n")
        md=["# Proof database — discrepancies","","**Generated:** "+str(date.today()),"","## Taxonomy","","| Kind | Meaning |","|------|---------|","| `missing_lemma` | Catalog cites absent Lean name |","| `open_vc` | Claimed proved but placeholder/sorry |","| `spec_drift` | Specimen/TOML vs Lean |","| `trusted_axiom` | Li.Trusted.* |","| `hardware_axiom` | G-hw FP limit |","","## Register","","| ID | Kind | Gap | Resolution |","|----|------|-----|------------|"]
        for x in rows: md.append(f"| `{x.id}` | `{x.kind}` | {x.gap_id or '—'} | {x.resolution} |")
        (od/"DISCREPANCIES.md").write_text("\n".join(md)+"\n"); print(f"Wrote proof-database/ ({len(rows)} rows)")
    elif not o.json: print(len(rows),"discrepancies")
    sys.exit(1 if len(rows)<o.min_count else 0)
if __name__=="__main__": main()
