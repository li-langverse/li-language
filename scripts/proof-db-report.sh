#!/usr/bin/env bash
# Proof-db discrepancy & gap reporter: compare JSONL sweeps or run vs expected.json.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASELINE="" RUN="" RUN_A="" RUN_B=""
REGISTRY="${ROOT}/proof-db/discrepancies.toml"
FORMAT="markdown" OUT_DIR="" ALLOW_DISCREPANCIES=0
usage(){ cat<<'EOF'
usage: proof-db-report.sh (--baseline PATH --run PATH | --a PATH --b PATH) [options]
  --format markdown|html|both  --out DIR  --registry PATH  --allow-discrepancies
Policy: proof-db/reporter.md
EOF
 exit "${1:-0}"; }
while [[ $# -gt 0 ]]; do case $1 in
 --baseline) BASELINE="$2";shift 2;; --run) RUN="$2";shift 2;;
 --a|--run-a) RUN_A="$2";shift 2;; --b|--run-b) RUN_B="$2";shift 2;;
 --registry) REGISTRY="$2";shift 2;; --format) FORMAT="$2";shift 2;;
 --out) OUT_DIR="$2";shift 2;; --allow-discrepancies) ALLOW_DISCREPANCIES=1;shift;;
 -h|--help) usage 0;; *) echo "unknown: $1" >&2; usage 1;; esac; done
[[ -n "$BASELINE" && -n "$RUN" && -z "$RUN_A" ]] && M=baseline || true
[[ -z "$BASELINE" && -n "$RUN_A" && -n "$RUN_B" ]] && M=ab || true
[[ -z "${M:-}" ]] && { echo "need (--baseline --run) or (--run-a --b|--run-b)" >&2; exit 1; }
export PROOF_DB_MODE=$M PROOF_DB_BASELINE=$BASELINE PROOF_DB_RUN=$RUN PROOF_DB_RUN_A=$RUN_A PROOF_DB_RUN_B=$RUN_B
export PROOF_DB_REGISTRY=$REGISTRY PROOF_DB_FORMAT=$FORMAT PROOF_DB_OUT_DIR=$OUT_DIR PROOF_DB_ALLOW=$ALLOW_DISCREPANCIES PROOF_DB_ROOT=$ROOT
python3 <<'PY'
import html,json,os,re,sys
from pathlib import Path
M,REG,FMT,OUT,ALLOW=os.environ["PROOF_DB_MODE"],Path(os.environ["PROOF_DB_REGISTRY"]),os.environ["PROOF_DB_FORMAT"],os.environ["PROOF_DB_OUT_DIR"],int(os.environ["PROOF_DB_ALLOW"])
P,O=set(["proved"]),set(["open","gap"])
def lj(p):
 d={}
 for i,l in enumerate(p.read_text().splitlines(),1):
  l=l.strip()
  if not l: continue
  r=json.loads(l); k=r.get("id")
  if not k: sys.exit(f"missing id {p}:{i}")
  if k in d: sys.exit(f"dup {k}")
  d[k]=r
 return d
def le(p):
 j=json.loads(p.read_text()); rows=j if isinstance(j,list) else j.get("records",[])
 return {x["id"]:x for x in rows if x.get("id")}
def lr(p):
 if not p.is_file(): return []
 o=[]
 for b in re.split(r"\n\[\[discrepancy\]\]",p.read_text())[1:]:
  g=lambda k:(m.group(1) if (m:=re.search(rf'^{k} = "([^"]*)"',b,re.M)) else "")
  o.append({"id":g("id"),"specimen":g("specimen"),"kind":g("kind"),"status":g("status") or "open","failure_mode":g("failure_mode"),"description":g("description")})
 return o
def di(row):
 f,rid=[],row["id"]; s,l,m=row.get("spec","?"),row.get("lean","?"),row.get("mir_witness","n/a"); fm=row.get("failure_mode")
 if (s in P and l in O) or (l in P and s in O): f.append({"id":rid,"kind":"spec_vs_lean","detail":f"spec={s} lean={l}","failure_mode":fm})
 if l in P and m in ("mismatch","absent"): f.append({"id":rid,"kind":"lean_vs_mir","detail":f"lean={l} mir={m}","failure_mode":fm})
 if s in P and m in ("mismatch","absent"): f.append({"id":rid,"kind":"spec_vs_mir","detail":f"spec={s} mir={m}","failure_mode":fm})
 return f
def dl(a,b):
 d,w=[],("spec","lean","mir_witness","open_goals","gap_id","failure_mode")
 for rid in sorted(set(a)|set(b)):
  if rid not in a: d.append({"id":rid,"kind":"run_delta","detail":"only B"});continue
  if rid not in b: d.append({"id":rid,"kind":"run_delta","detail":"only A"});continue
  for fld in w:
   va,vb=a[rid].get(fld),b[rid].get(fld)
   if va!=vb: d.append({"id":rid,"kind":"run_delta","detail":f"{fld}:{va!r}->{vb!r}","failure_mode":b[rid].get("failure_mode") or a[rid].get("failure_mode")})
 return d
def gp(rows):
 return [{"id":k,"gap_id":v.get("gap_id") or "","spec":v.get("spec"),"lean":v.get("lean"),"open_goals":int(v.get("open_goals") or 0),"failure_mode":v.get("failure_mode") or ""}
         for k,v in sorted(rows.items()) if v.get("gap_id") or v.get("lean") in O or v.get("spec") in O or int(v.get("open_goals") or 0)>0]
def pc(rows):
 n=len(rows) or 1
 return 100*sum(1 for r in rows.values() if r.get("lean") in P)/n,100*sum(1 for r in rows.values() if r.get("spec") in P)/n,sum(1 for r in rows.values() if r.get("lean") in P and r.get("spec") in P)
def cv(reg,d):
 return any(r.get("status")=="accepted" and r.get("specimen")==d.get("id") and r.get("kind")==d.get("kind") for r in reg)
reg=lr(REG)
if M=="baseline":
 base=le(Path(os.environ["PROOF_DB_BASELINE"])); pri=lj(Path(os.environ["PROOF_DB_RUN"])); lab="baseline vs run"; delt=dl(base,pri)
else:
 a=lj(Path(os.environ["PROOF_DB_RUN_A"])); pri=lj(Path(os.environ["PROOF_DB_RUN_B"])); lab="A vs B"; delt=dl(a,pri)
auto=[x for row in pri.values() for x in di(row)]; un=[x for x in auto if not cv(reg,x)]; lp,sp,both=pc(pri)
def tb(h,rows):
 if not rows: return "_None._\n"
 L=["| "+" | ".join(h)+" |","| "+" | ".join("---" for _ in h)+" |"]+["| "+" | ".join(str(r.get(x,"")) for x in h)+" |" for r in rows]
 return "\n".join(L)+"\n"
md=["# Proof database report","","**Compare:** "+lab,"","## Summary","",f"- Specimens: {len(pri)}",f"- Lean proved: {lp:.1f}%",f"- Spec proved: {sp:.1f}%",f"- Both proved: {both}/{len(pri)}",f"- Discrepancies: {len(auto)} ({len(un)} unresolved)","","## Open gaps","",tb(["id","gap_id","spec","lean","open_goals","failure_mode"],gp(pri)),"## Discrepancies","",tb(["id","kind","detail","failure_mode"],auto)]
if delt: md+=["## Run deltas","",tb(["id","kind","detail","failure_mode"],delt)]
md+=["## Policy","","| Mode | Meaning |","| li_bug | Toolchain |","| wrong_spec | User contract |","| open_math | Open VC |","| axiomatic_limit | Limit |",""]
mt="\n".join(md)
def ht():
 def t(h,rows):
  if not rows: return "<p>None</p>"
  th="".join(f"<th>{html.escape(x)}</th>" for x in h)
  body="".join("<tr>"+"".join(f"<td>{html.escape(str(r.get(x,'')))}</td>" for x in h)+"</tr>" for r in rows)
  return f"<table><thead><tr>{th}</tr></thead><tbody>{body}</tbody></table>"
 s="body{font-family:system-ui;margin:2rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ccc;padding:.4rem}"
 return f"<!DOCTYPE html><html><head><meta charset=utf-8><style>{s}</style></head><body><h1>Proof DB</h1><p>{html.escape(lab)}</p>{t(['id','kind','detail','failure_mode'],auto)}</body></html>"
h=ht()
if OUT:
 o=Path(OUT);o.mkdir(parents=True,exist_ok=True)
 if FMT in ("markdown","both"): (o/"report.md").write_text(mt); print("wrote",o/"report.md")
 if FMT in ("html","both"): (o/"report.html").write_text(h); print("wrote",o/"report.html")
else: print(h if FMT=="html" else mt)
if un and not ALLOW: print(f"FAIL {len(un)} unresolved",file=sys.stderr); sys.exit(1)
print("proof-db-report: ok",file=sys.stderr)
PY
