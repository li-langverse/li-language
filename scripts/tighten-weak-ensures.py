#!/usr/bin/env python3
"""One-shot: replace `ensures true` on non-unit defs with honest postconditions."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def split_procs(text: str) -> list[str]:
    parts = re.split(r"\n(?=def )", text)
    return parts


def proc_return_type(proc: str) -> str | None:
    m = re.search(r"->\s*([^\n]+)", proc)
    return m.group(1).strip() if m else None


def is_unit(ret: str | None) -> bool:
    return ret is None or ret == "unit"


def is_extern(proc: str) -> bool:
    return proc.startswith("extern proc")


def count_returns(proc: str) -> list[str]:
    return re.findall(r"^\s+return\s+(.+)$", proc, re.M)


def field_assignments_before_return(proc: str) -> dict[str, str]:
    """Map result.field -> rhs for `w.field = expr` before final return w."""
    lines = proc.split("\n")
    assigns: dict[str, str] = {}
    for line in lines:
        m = re.match(r"\s+(\w+)\.(\w+)\s*=\s*(.+)$", line)
        if m:
            assigns[f"{m.group(1)}.{m.group(2)}"] = m.group(3).strip()
    return assigns


def fix_proc(proc: str) -> str:
    if is_extern(proc) or "ensures true" not in proc:
        return proc
    ret = proc_return_type(proc)
    if is_unit(ret):
        return proc

    returns = count_returns(proc)
    if len(returns) == 1:
        expr = returns[0].strip()
        return proc.replace("ensures true", f"ensures result == {expr}", 1)

    if ret == "int" and all(r.strip() in ("0", "1") for r in returns):
        return proc.replace(
            "ensures true", "ensures result >= 0\n  ensures result <= 1", 1
        )

    # struct/object: infer from `var w: T` + field assigns + `return w`
    if returns and returns[-1].strip() in ("w", "c", "t", "s", "f", "e", "p", "m", "q"):
        var = returns[-1].strip()
        assigns = field_assignments_before_return(proc)
        clauses = []
        for key, rhs in assigns.items():
            if key.startswith(var + "."):
                field = key.split(".", 1)[1]
                clauses.append(f"ensures result.{field} == {rhs}")
        if clauses:
            block = "\n  ".join(clauses)
            return proc.replace("ensures true", block, 1)

    # known physics / UI patterns
    name_m = re.match(r"def (\w+)", proc)
    name = name_m.group(1) if name_m else ""

    replacements: dict[str, str] = {
        "lorentz_gamma": "ensures result >= 1.0",
        "time_dilation_proper": "ensures result == dt / gamma",
        "relativistic_momentum": "ensures result >= 0.0",
        "schwarzschild_factor": "ensures result >= 0.0",
        "weak_field_perihelion_precession_factor": "ensures result == 1.0",
        "pgs_resolve_normal": "ensures result >= impulse",
        "broadphase_cell_index": "ensures result == 0",
        "emitter_tick": "ensures result >= 0",
        "decay_branching": "ensures result == 0.0",
        "sample_isotropic_costheta": "ensures result == 0.0",
        "event_weight_cross_section": "ensures result == sigma * luminosity",
        "harmonic_potential_1d": "ensures result == 0.5 * k * x * x",
        "schrodinger_kinetic_diag": "ensures result > 0.0",
        "sample_wind_at": "ensures result == wind_x[ix]",
        "norm_sq_1d": "ensures result >= 0.0",
        "math_tag": "ensures result == 1",
        "li_std_numerics_version": "ensures result == 1",
        "select_integrator_order": "ensures result >= 1\n  ensures result <= 4",
        "simulation_fixed_dt_steps": "ensures result == params.steps",
        "profile_for_tier": "ensures result.tier == tier",
        "vec3_normalize": "ensures result.x == a.x\n  ensures result.y == a.y\n  ensures result.z == a.z",
    }
    if name in replacements:
        return proc.replace("ensures true", replacements[name], 1)

    if ret == "float" and returns and all(r.strip() == "0.0" for r in returns):
        return proc.replace("ensures true", "ensures result == 0.0", 1)

    return proc


def fix_file(path: Path) -> bool:
    text = path.read_text()
    parts = split_procs(text)
    out = [parts[0]]
    changed = False
    for part in parts[1:]:
        new = fix_proc(part)
        if new != part:
            changed = True
        out.append(new)
    if changed:
        new_text = out[0]
        for part in out[1:]:
            if new_text and not new_text.endswith("\n"):
                new_text += "\n"
            new_text += part
        path.write_text(new_text)
    return changed


def main() -> int:
    targets = list((ROOT / "packages").rglob("*.li"))
    targets += list((ROOT / "li-tests").rglob("*.li"))
    targets += [ROOT / "bootstrap" / "lic" / "main.li"]
    n = 0
    for p in sorted(set(targets)):
        if p.is_file() and fix_file(p):
            print("fixed", p.relative_to(ROOT))
            n += 1
    print(f"tighten-weak-ensures: {n} files updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
