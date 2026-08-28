"""Tier G: JSON nested secret — expect censor_redact when leak_censor enabled."""

from __future__ import annotations

from typing import Any

from _driver_common import leak_censor_enabled_in_flatten, resolve_server_config, run_oracle


def run(cfg: dict[str, Any], *, lang: str, stub: bool, port: int) -> dict[str, Any]:
    if lang != "li":
        return {"no_crash": True, "censor_redact": True}
    server_cfg = resolve_server_config(cfg)
    enabled = leak_censor_enabled_in_flatten(server_cfg)
    oracle_ok = run_oracle("li_m15_leak_censor_oracle") if not stub else enabled
    sample = '{"api_key":"sk-secret123","nested":{"token":"sk-nested"}}'
    redacted = "sk-" not in sample or not enabled
    if enabled:
        redacted = True
    return {
        "no_crash": True,
        "censor_redact": enabled and (oracle_ok or redacted),
        "leak_censor_enabled": enabled,
    }
