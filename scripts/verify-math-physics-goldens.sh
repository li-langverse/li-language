#!/usr/bin/env bash
# Small-N normative goldens for math (@, sum) and physics stubs — not parity with other langs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

export CC="${CC:-clang-22}"
export CXX="${CXX:-clang++-22}"
export LIC="${LIC:-$ROOT/build/compiler/lic/lic}"

if [[ ! -x "$LIC" ]]; then
  li_fail "missing lic — run ./scripts/build.sh"
  exit 1
fi

li_phase "math/physics golden verify (spec + small Li runs)"

python3 <<PY
import subprocess
import sys
from pathlib import Path

ROOT = Path("${ROOT}")
sys.path.insert(0, str(ROOT / "benchmarks" / "harness"))
from reference import TIER1_REFERENCE, float_close, format_result, parse_result

# --- Python normative spec (small) -------------------------------------------
ref = TIER1_REFERENCE["simd_dot"]
dot8 = format_result(ref.compute_small())
mm4 = format_result(TIER1_REFERENCE["matmul_naive"].compute_small())
mb8 = format_result(TIER1_REFERENCE["matmul_blocked"].compute_small())

def run_li_golden(rel: str, expected: str, *, rtol: float = 0.0) -> None:
    src = ROOT / "li-tests" / rel
    out = ROOT / "build" / "goldens" / rel.replace("/", "_").replace(".li", "")
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.check_call(
        [
            str(ROOT / "build/compiler/lic/lic"),
            "build",
            str(src),
            "-o",
            str(out),
            "--allow-open-vc",
            "--no-lean-verify",
            "--release",
            "-O3",
            "-ffast-math",
            "-march=native",
        ],
        cwd=ROOT,
    )
    proc = subprocess.run(
        [str(out)],
        capture_output=True,
        text=True,
        env={**__import__("os").environ, "LI_PRINT_SINK_F64": "1"},
        check=False,
    )
    if proc.returncode != 0:
        raise SystemExit(f"{rel}: run failed rc={proc.returncode}")
    lines = (proc.stdout or "").strip().splitlines()
    if not lines:
        raise SystemExit(f"{rel}: no sink output")
    got = lines[-1].strip()
    if rtol > 0.0:
        exp_f = float(expected) if expected not in ("inf", "-inf") else parse_result(expected)
        if not float_close(parse_result(got), exp_f, rtol=rtol):
            raise SystemExit(f"{rel}: got {got!r} expected ~{expected!r} (rtol={rtol})")
    elif got != expected:
        raise SystemExit(f"{rel}: got {got!r} expected {expected!r}")

run_li_golden("math_linalg/golden_dot4_ones_twos.li", "8")
run_li_golden("math_linalg/golden_tier1_dot8.li", dot8, rtol=1e-14)
run_li_golden("physics/golden_positions_sum.li", "3")

print(f"spec small: simd_dot(8)={dot8} matmul_naive(4)={mm4} matmul_blocked(8)={mb8}")
print("math/physics golden verify ok")
PY

li_ok "math/physics golden verify finished"
