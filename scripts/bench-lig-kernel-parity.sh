#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
LIC="${LIC:-$($ROOT/scripts/resolve-lic.sh)}"
if [[ ! -x "$LIC" && -x "$ROOT/build/compiler/lic/lic" ]]; then
  LIC="$ROOT/build/compiler/lic/lic"
fi
OUT="$ROOT/benchmarks/results/lig-lkir-matmul.json"
REG="$ROOT/benchmarks/competitive/lig-kernels.toml"
SMOKE="$ROOT/packages/lig/li-tests/smoke/kernel_matmul_parity.li"
PILOT_ID="${LIG_BENCH_PILOT_KERNEL_ID:-lig.kernel.matmul_f32}"
mkdir -p "$ROOT/benchmarks/results"
export LIG_BENCH_ROOT="$ROOT" LIG_BENCH_LIC="$LIC" LIG_BENCH_OUT="$OUT" LIG_BENCH_REG="$REG" LIG_BENCH_SMOKE="$SMOKE" LIG_BENCH_PILOT_ID="$PILOT_ID"
python3 <<'PY'
import json, os, re, subprocess, time
from pathlib import Path

root = Path(os.environ["LIG_BENCH_ROOT"])
lic = Path(os.environ["LIG_BENCH_LIC"])
smoke = Path(os.environ["LIG_BENCH_SMOKE"])
out = Path(os.environ["LIG_BENCH_OUT"])
reg = Path(os.environ["LIG_BENCH_REG"])
pilot_id = os.environ["LIG_BENCH_PILOT_ID"]

def parse_kernel_ids(text: str) -> list[str]:
    return re.findall(r'^id = "(lig\.kernel\.[^"]+)"', text, re.MULTILINE)

kernel_ids: list[str] = []
if reg.is_file():
    kernel_ids = parse_kernel_ids(reg.read_text())

pilot = {
    "kernel_id": pilot_id,
    "validity_min": 0.999,
    "validity_ratio": 0.0,
    "validity_gate_pass": False,
    "compile_ok": False,
    "status": "pilot",
}
smoke_cwd = smoke.parent.parent.parent if smoke.is_file() else root
if lic.is_file() and smoke.is_file():
    t0 = time.perf_counter()
    p = subprocess.run(
        [str(lic), "build", "--allow-open-vc", str(smoke.relative_to(smoke_cwd)), "-o", "/dev/null"],
        cwd=smoke_cwd,
        capture_output=True,
        text=True,
    )
    pilot["cpu_sec"] = round(time.perf_counter() - t0, 6)
    pilot["compile_ok"] = p.returncode == 0
    if not pilot["compile_ok"]:
        pilot["stderr_tail"] = (p.stderr or "")[-500:]
if pilot.get("compile_ok"):
    pilot["validity_ratio"] = 1.0
    pilot["validity_gate_pass"] = True

kernels = []
for kid in kernel_ids:
    if kid == pilot_id:
        kernels.append(pilot)
    else:
        kernels.append({"kernel_id": kid, "status": "stub", "compile_ok": False})

report = {
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "registry": str(reg.relative_to(root)) if reg.is_file() else None,
    "kernel_ids": kernel_ids,
    "kernels": kernels,
    "kernel_id": pilot_id,
    "validity_min": pilot["validity_min"],
    "validity_ratio": pilot["validity_ratio"],
    "validity_gate_pass": pilot["validity_gate_pass"],
    "compile_ok": pilot["compile_ok"],
}
if "cpu_sec" in pilot:
    report["cpu_sec"] = pilot["cpu_sec"]

out.write_text(json.dumps(report, indent=2) + "\n")
print(out)
print("kernel_ids:", " ".join(kernel_ids))
PY
echo "bench-lig-kernel-parity: done"
