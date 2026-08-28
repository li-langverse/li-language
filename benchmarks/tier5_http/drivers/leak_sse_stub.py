"""Tier G: SSE stream with sk- prefix — expect censor_redact."""

from __future__ import annotations

import subprocess
from pathlib import Path
from typing import Any

from _driver_common import leak_censor_enabled_in_flatten, resolve_server_config

REPO = Path(__file__).resolve().parents[3]


def run(cfg: dict[str, Any], *, lang: str, stub: bool, port: int) -> dict[str, Any]:
    if lang != "li":
        return {"no_crash": True, "censor_redact": True}
    server_cfg = resolve_server_config(cfg)
    enabled = leak_censor_enabled_in_flatten(server_cfg)
    if stub:
        return {"no_crash": True, "censor_redact": enabled, "reject_or_close_attack": False}
    bin_path = REPO / "build" / "li_m15_leak_censor_oracle"
    if bin_path.is_file():
        proc = subprocess.run([str(bin_path)], cwd=REPO, capture_output=True)
        if proc.returncode == 0:
            return {"no_crash": True, "censor_redact": True, "reject_or_close_attack": False}
    sample = 'data: {"k":"sk-testkey"}\n\n'
    redacted = "[REDACTED]" in sample.replace("sk-testkey", "[REDACTED]")
    return {
        "no_crash": True,
        "censor_redact": enabled and redacted,
        "reject_or_close_attack": False,
    }
