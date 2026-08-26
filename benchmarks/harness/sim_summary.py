#!/usr/bin/env python3
"""Write li_sim_summary_v1 summaries for tier-2 smokes, verify.py, and Li sim runs."""

from __future__ import annotations

import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Literal

REPO = Path(__file__).resolve().parents[2]
RESULTS = REPO / "benchmarks" / "results"
REGISTRY_PATH = REPO / "benchmarks" / "competitive" / "algo_registry.json"
SCHEMA = "li_sim_summary_v1"

SummaryFormat = Literal["json", "json_min", "yaml"]
SUMMARY_FORMATS: tuple[SummaryFormat, ...] = ("json", "json_min", "yaml")

VERTICAL_BY_INT: dict[int, str] = {
    1: "md_lennard_jones",
    2: "pde_heat_2d",
    3: "gaming_rigid",
    4: "qm_dft",
}


def git_sha() -> str:
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "--short", "HEAD"], cwd=REPO, text=True
            )
            .strip()
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def params_digest(params_path: Path | None) -> str | None:
    if params_path is None or not params_path.is_file():
        return None
    h = hashlib.sha256(params_path.read_bytes()).hexdigest()
    return f"sha256:{h}"


def load_algo_registry() -> dict[str, Any]:
    if not REGISTRY_PATH.is_file():
        return {"schema": "li_algo_registry_v1", "count": 0, "algorithms": []}
    return json.loads(REGISTRY_PATH.read_text())


def algo_entry(algo_id: int) -> dict[str, Any] | None:
    for row in load_algo_registry().get("algorithms", []):
        if int(row.get("id", -1)) == algo_id:
            return row
    return None


def vertical_id_for_benchmark(benchmark: str) -> str:
    mapping = {
        "md_lennard_jones": "md_lennard_jones",
        "heat_equation_2d": "pde_heat_2d",
    }
    return mapping.get(benchmark, benchmark)


def vertical_id_from_int(vertical_id: int) -> str:
    return VERTICAL_BY_INT.get(vertical_id, f"vertical_{vertical_id}")


def detail_name(detail: str | int) -> str:
    if isinstance(detail, int):
        return ("summary", "fields", "debug", "repro")[detail] if 0 <= detail <= 3 else "summary"
    return detail


def metrics_for_benchmark(benchmark: str, verify_line: str) -> dict[str, Any]:
    value = float(verify_line.strip())
    if benchmark == "md_lennard_jones":
        return {
            "energy_drift_rel": value,
            "checksum": verify_line.strip(),
        }
    if benchmark == "heat_equation_2d":
        return {
            "checksum": verify_line.strip(),
            "stencil_checksum": value,
        }
    return {"checksum": verify_line.strip()}


def invariants_for_benchmark(benchmark: str, *, passed: bool) -> dict[str, bool]:
    if benchmark == "md_lennard_jones":
        return {"energy_drift_ok": passed}
    return {"checksum_ok": passed}


def artifacts_block(
    benchmark: str,
    *,
    detail: str,
    params_path: Path | None = None,
) -> dict[str, Any]:
    params = params_path or REPO / "benchmarks" / "tier2_physics" / benchmark / "params.toml"
    artifacts: dict[str, Any] = {
        "params": str(params.relative_to(REPO)) if params.is_file() else None,
        "tier_f": None,
        "tier_d": None,
    }
    if detail in ("fields", "debug", "repro"):
        artifacts["tier_f"] = f"benchmarks/results/{benchmark}/fields/"
    if detail in ("debug", "repro"):
        artifacts["tier_d"] = f"benchmarks/results/{benchmark}/tier_d/"
    if detail == "repro":
        artifacts["repro_bundle"] = f"benchmarks/results/{benchmark}/publish/"
    return artifacts


def build_summary(
    *,
    benchmark: str,
    lang: str,
    variant: str,
    verify_line: str,
    passed: bool,
    detail: str,
    params_path: Path | None = None,
    workload_class: str = "stub",
    algo_id: int | None = None,
) -> dict[str, Any]:
    detail_s = detail_name(detail)
    entry = algo_entry(algo_id) if algo_id is not None else None
    return {
        "schema": SCHEMA,
        "benchmark": benchmark,
        "vertical_id": vertical_id_for_benchmark(benchmark),
        "algo_id": algo_id if algo_id is not None else entry.get("id") if entry else None,
        "algo_name": entry.get("name") if entry else None,
        "workload_class": workload_class,
        "lang": lang,
        "variant": variant,
        "output_detail": detail_s,
        "ok": passed,
        "git_sha": git_sha(),
        "cpu_model": "",
        "flags": "",
        "params_digest": params_digest(params_path if params_path and params_path.is_file() else None),
        "metrics": metrics_for_benchmark(benchmark, verify_line),
        "invariants": invariants_for_benchmark(benchmark, passed=passed),
        "artifacts": artifacts_block(benchmark, detail=detail_s, params_path=params_path),
        "updated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def build_summary_from_li_run(
    *,
    algo_id: int,
    ok: bool,
    checksum: float,
    vertical_id: int,
    detail: str | int = "summary",
    lang: str = "li",
    variant: str = "pure_li",
    workload_class: str | None = None,
    benchmark: str | None = None,
) -> dict[str, Any]:
    """Same shape as verify.py summaries for composable / sim.scientific runs."""
    detail_s = detail_name(detail)
    entry = algo_entry(algo_id)
    bench = benchmark or (entry["name"] if entry else f"algo_{algo_id}")
    vert = vertical_id_from_int(vertical_id)
    wl = workload_class
    if wl is None:
        wl = "smoke" if entry and entry.get("implemented_smoke") else "registry_stub"
    verify_line = str(checksum)
    params_path = REPO / "benchmarks" / "tier2_physics" / bench / "params.toml"
    if not params_path.is_file():
        params_path = None
    summary = {
        "schema": SCHEMA,
        "benchmark": bench,
        "vertical_id": vert,
        "algo_id": algo_id,
        "algo_name": entry.get("name") if entry else None,
        "workload_class": wl,
        "lang": lang,
        "variant": variant,
        "output_detail": detail_s,
        "ok": ok,
        "git_sha": git_sha(),
        "cpu_model": "",
        "flags": "",
        "params_digest": params_digest(params_path) if params_path else None,
        "metrics": {"checksum": verify_line, "checksum_f64": checksum},
        "invariants": {"checksum_ok": ok},
        "artifacts": artifacts_block(bench, detail=detail_s, params_path=params_path),
        "updated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    if bench in ("md_lennard_jones", "md_lj_cutoff_mic"):
        summary["metrics"]["energy_drift_rel"] = checksum
        summary["invariants"] = {"energy_drift_ok": ok}
    return summary


def summary_suffix(fmt: SummaryFormat) -> str:
    if fmt == "json_min":
        return ".summary.min.json"
    if fmt == "yaml":
        return ".summary.yaml"
    return ".summary.json"


def default_summary_path(benchmark: str, lang: str, fmt: SummaryFormat = "json") -> Path:
    base = f"{lang}{summary_suffix(fmt)}"
    return RESULTS / benchmark / base


def serialize_summary(summary: dict[str, Any], fmt: SummaryFormat) -> str:
    if fmt == "json":
        return json.dumps(summary, indent=2) + "\n"
    if fmt == "json_min":
        return json.dumps(summary, separators=(",", ":")) + "\n"
    return _dump_yaml(summary)


def _yaml_scalar(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return json.dumps(value)
    s = str(value)
    if s == "" or any(c in s for c in ":#[]{}\"'\\,\n\t"):
        return json.dumps(s)
    return s


def _dump_yaml(obj: Any, indent: int = 0) -> str:
    pad = "  " * indent
    if isinstance(obj, dict):
        lines: list[str] = []
        for key, val in obj.items():
            if isinstance(val, (dict, list)) and val:
                lines.append(f"{pad}{key}:")
                lines.append(_dump_yaml(val, indent + 1).rstrip("\n"))
            else:
                lines.append(f"{pad}{key}: {_yaml_scalar(val)}")
        return "\n".join(lines) + "\n"
    if isinstance(obj, list):
        lines = []
        for item in obj:
            if isinstance(item, (dict, list)):
                lines.append(f"{pad}-")
                lines.append(_dump_yaml(item, indent + 1).rstrip("\n"))
            else:
                lines.append(f"{pad}- {_yaml_scalar(item)}")
        return "\n".join(lines) + "\n"
    return f"{pad}{_yaml_scalar(obj)}\n"


def write_summary(path: Path, summary: dict[str, Any], fmt: SummaryFormat = "json") -> Path:
    """Write summary; extension normalized from fmt when path has generic suffix."""
    path = summary_path_for_format(path, fmt)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(serialize_summary(summary, fmt))
    return path


def summary_path_for_format(path: Path, fmt: SummaryFormat) -> Path:
    for old in (".summary.json", ".summary.min.json", ".summary.yaml"):
        if path.name.endswith(old):
            base = path.name[: -len(old)]
            return path.parent / (base + summary_suffix(fmt))
    return path


def parse_summary_file(path: Path) -> dict[str, Any]:
    text = path.read_text()
    if path.suffix in (".yaml", ".yml"):
        try:
            import yaml  # type: ignore

            return yaml.safe_load(text)
        except ImportError:
            raise RuntimeError("PyYAML required to parse .summary.yaml") from None
    return json.loads(text)
