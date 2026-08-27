"""Tier G: censorship disabled — expect passthrough (documented insecure)."""

from __future__ import annotations

from typing import Any

from _driver_common import leak_censor_enabled_in_flatten, resolve_server_config


def run(cfg: dict[str, Any], *, lang: str, stub: bool, port: int) -> dict[str, Any]:
    if lang != "li":
        return {"no_crash": True, "censor_redact": False, "documented_insecure": True}
    server_cfg = resolve_server_config(cfg)
    enabled = leak_censor_enabled_in_flatten(server_cfg)
    return {
        "no_crash": True,
        "censor_redact": False if not enabled else True,
        "documented_insecure": not enabled,
    }
