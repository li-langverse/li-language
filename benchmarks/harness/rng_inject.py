"""Merge exploit [server.rng] into li-httpd config / env for Tier F harness."""

from __future__ import annotations

import os
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[2]
SCRIPTS = REPO / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from httpd_bad_rng import driver_from_rng_table  # noqa: E402
from httpd_rng import validate_rng_config  # noqa: E402

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib  # type: ignore


def exploit_has_rng(cfg: dict[str, Any]) -> bool:
    server = cfg.get("server")
    return isinstance(server, dict) and isinstance(server.get("rng"), dict)


def merge_rng_into_config(base: dict[str, Any], exploit: dict[str, Any]) -> dict[str, Any]:
    merged = deepcopy(base)
    server = merged.setdefault("server", {})
    if not isinstance(server, dict):
        server = {}
        merged["server"] = server
    rng = exploit.get("server", {})
    if isinstance(rng, dict):
        rng = rng.get("rng")
    if isinstance(exploit.get("server"), dict) and isinstance(exploit["server"].get("rng"), dict):
        rng = exploit["server"]["rng"]
    if isinstance(rng, dict):
        server["rng"] = deepcopy(rng)
    return merged


def write_overlay_config(
    base_path: Path,
    exploit_cfg: dict[str, Any],
    *,
    out_dir: Path | None = None,
) -> Path:
    base_text = base_path.read_text(encoding="utf-8")
    base = tomllib.loads(base_text)
    merged = merge_rng_into_config(base, exploit_cfg)
    os.environ["LI_HTTPD_EXPLOIT_HARNESS"] = "1"
    errs, _warn = validate_rng_config(merged)
    if errs:
        raise ValueError("; ".join(errs))
    dest_dir = out_dir or (REPO / "build" / "tier5_rng_overlay")
    dest_dir.mkdir(parents=True, exist_ok=True)
    eid = str(exploit_cfg.get("id", "exploit"))
    out = dest_dir / f"{eid}_rng.toml"
    rng = (merged.get("server") or {}).get("rng") if isinstance(merged.get("server"), dict) else None
    overlay = _render_server_rng(rng or {})
    out.write_text(base_text.rstrip() + "\n\n" + overlay, encoding="utf-8")
    return out


def _render_server_rng(rng: dict[str, Any]) -> str:
    lines = ["[server.rng]"]
    for key in (
        "mode",
        "bad_pattern",
        "bad_seed",
        "seed",
        "sim_schedule_file",
        "allow_insecure_rng",
    ):
        if key not in rng:
            continue
        val = rng[key]
        if isinstance(val, bool):
            lines.append(f"{key} = {'true' if val else 'false'}")
        elif isinstance(val, str):
            lines.append(f'{key} = "{val}"')
        else:
            lines.append(f"{key} = {val}")
    return "\n".join(lines)


def rng_driver_for_exploit(cfg: dict[str, Any]):
    server = cfg.get("server") or {}
    rng = server.get("rng") if isinstance(server, dict) else None
    if not isinstance(rng, dict):
        return None
    return driver_from_rng_table(rng)


def exploit_env(cfg: dict[str, Any]) -> dict[str, str]:
    env = {"LI_HTTPD_EXPLOIT_HARNESS": "1"}
    server = cfg.get("server") or {}
    rng = server.get("rng") if isinstance(server, dict) else None
    if isinstance(rng, dict):
        env["LI_HTTPD_RNG_MODE"] = str(rng.get("mode") or "os")
        if rng.get("bad_pattern"):
            env["LI_HTTPD_RNG_BAD_PATTERN"] = str(rng["bad_pattern"])
        if rng.get("bad_seed") is not None:
            env["LI_HTTPD_RNG_BAD_SEED"] = str(rng["bad_seed"])
        if rng.get("seed") is not None:
            env["LI_HTTPD_RNG_SEED"] = str(rng["seed"])
    return env
