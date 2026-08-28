"""Shared helpers for tier5_http exploit stub drivers."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[3]
SCRIPTS = REPO / "scripts"


def flatten_config(server_config: Path) -> str:
    tmp = Path("/tmp") / f"li_exploit_flatten_{server_config.stem}.conf"
    cmd = [
        sys.executable,
        str(SCRIPTS / "flatten-httpd-config.py"),
        str(server_config.resolve()),
        "-o",
        str(tmp),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr or proc.stdout or "flatten failed")
    return tmp.read_text(encoding="utf-8")


def leak_censor_enabled_in_flatten(server_config: Path) -> bool:
    text = flatten_config(server_config)
    for line in text.splitlines():
        if line.startswith("leak_censor_enabled="):
            val = line.split("=", 1)[1].strip()
            return val not in ("0", "false")
    return False


def run_oracle(name: str) -> bool:
    path = REPO / "build" / name
    if not path.is_file():
        path = Path(f"/tmp/{name}")
    if not path.is_file() or not path.stat().st_mode & 0o111:
        return False
    proc = subprocess.run([str(path)], cwd=REPO, capture_output=True)
    return proc.returncode == 0


def resolve_server_config(cfg: dict[str, Any]) -> Path:
    from http_exploit_toml import resolve_server_config as _resolve

    return _resolve(cfg)
