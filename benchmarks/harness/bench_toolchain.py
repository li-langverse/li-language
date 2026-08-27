#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, os, platform, statistics, subprocess, sys, time
from pathlib import Path
REPO=Path(__file__).resolve().parents[2]
MICRO=REPO/"benchmarks/toolchain/micro"
LIC=REPO/"build/compiler/lic/lic"
OUT=REPO/"benchmarks/results/toolchain_latest.csv"
HDR=["tool","command","corpus","jobs","wall_s","peak_rss_mb","exit_code","diagnostics_count","cache_hit","git_sha","cpu_model","flags"]

def timed(cmd,runs=1):
 s=[];c=0
 for _ in range(max(1,runs)):
  t=time.perf_counter();p=subprocess.run(cmd,cwd=REPO,capture_output=True,text=True);s.append(time.perf_counter()-t);c=p.returncode
 return statistics.median(s),c

def main():
 a=argparse.ArgumentParser();a.add_argument("--smoke",action="store_true");a.add_argument("--runs",type=int,default=1);args=a.parse_args()
 if not LIC.is_file(): raise SystemExit(f"lic missing at {LIC} — run ./scripts/build.sh")
 rows=[]
 for p in sorted(MICRO.glob("*.li")):
  cmd=[str(LIC),"check",str(p),"--format=json"];w,c=timed(cmd,runs=1)
  rows.append({k:"" for k in HDR}|{"tool":"lic","command":" ".join(cmd),"corpus":"micro/"+p.name,"jobs":1,"wall_s":round(w,4),"exit_code":c,"diagnostics_count":0,"cache_hit":"false","git_sha":"dev","cpu_model":platform.processor() or "","flags":"--smoke"})
  print(p.name,w,c)
 OUT.parent.mkdir(parents=True,exist_ok=True)
 with OUT.open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=HDR);w.writeheader();w.writerows(rows)
 print("wrote",OUT,len(rows))
 return 0
if __name__=="__main__": raise SystemExit(main())
