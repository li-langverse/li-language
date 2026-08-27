"""Tier F: validate-httpd-config with [server.rng] overlay from exploit TOML."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[3]
SCRIPTS = REPO / "scripts"
HARNESS = REPO / "benchmarks" / "harness"
for p in (str(SCRIPTS), str(HARNESS)):
    if p not in sys.path:
        sys.path.insert(0, p)

from rng_inject import write_overlay_config  # noqa: E402
from http_exploit_toml import resolve_server_config  # noqa: E402


def run(cfg: dict[str, Any], *, lang: str, stub: bool, port: int) -> dict[str, Any]:
    if lang != "li":
        return {"no_crash": True, "validate_config_fails": False}
    server_cfg = resolve_server_config(cfg)
    from rng_inject import exploit_has_rng

    try:
        if exploit_has_rng(cfg):
            overlay = write_overlay_config(server_cfg, cfg)
        else:
            overlay = server_cfg
    except ValueError as exc:
        return {
            "no_crash": True,
            "validate_config_fails": True,
            "reject_or_fail": True,
            "message": str(exc),
        }
    script = REPO / "scripts" / "validate-httpd-config.py"
    proc = subprocess.run(
        [sys.executable, str(script), str(overlay)],
        capture_output=True,
        text=True,
    )
    failed = proc.returncode != 0
    return {
        "no_crash": True,
        "validate_config_fails": failed,
        "reject_or_fail": failed,
        "stderr": (proc.stderr or "").strip()[:200],
    }
