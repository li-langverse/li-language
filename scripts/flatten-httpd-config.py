#!/usr/bin/env python3
"""Flatten validated li-httpd.toml to httpd.runtime.conf for C loader.

Usage:
  python3 scripts/flatten-httpd-config.py li-tests/config_desugar/good/agent_gateway.toml \\
    -o /tmp/httpd.runtime.conf
  ./build/li-httpd /tmp/httpd.runtime.conf
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib  # type: ignore

from httpd_config import ConfigError, load_httpd_config
from httpd_leak_censor import (
    PATTERN_IDS,
    generated_paths_for_config,
    leak_censor_enabled,
)
from httpd_m15 import ConfigError as M15Error, parse_duration, validate_route_match
from httpd_m2 import ConfigError as M2Error, m2_flatten_lines
from httpd_m3 import ConfigError as M3Error, m3_flatten_lines
from httpd_tls import ConfigError as TlsError, tls_flatten_lines


def parse_listen(raw: str) -> int:
    raw = raw.strip()
    if raw.startswith(":"):
        return int(raw[1:])
    if ":" in raw:
        return int(raw.rsplit(":", 1)[1])
    return int(raw)


def peer_port(url: str) -> int:
    m = re.match(r"https?://[^:]+:(\d+)", url.strip())
    if not m:
        raise ValueError(f"peer URL must be loopback with port: {url!r}")
    return int(m.group(1))


def flatten(cfg_path: Path) -> list[str]:
    data = tomllib.loads(cfg_path.read_text(encoding="utf-8"))
    lines: list[str] = []
    server = data.get("server") or {}
    listen = server.get("listen")
    if listen:
        lines.append(f"listen_port={parse_listen(str(listen))}")
    root = server.get("document_root")
    if root:
        rp = Path(str(root))
        if not rp.is_absolute():
            rp = (cfg_path.parent / rp).resolve()
        lines.append(f"document_root={rp}")
    workers = server.get("workers")
    if workers is not None:
        w = str(workers).strip().lower()
        if w in ("auto", ""):
            lines.append("workers=auto")
        else:
            lines.append(f"workers={int(workers)}")

    auth = data.get("auth") or {}
    if isinstance(auth, dict):
        req = auth.get("require_bearer")
        if req is not None:
            on = str(req).lower() not in ("0", "false", "no")
            lines.append(f"auth_required={1 if on else 0}")
        keys = auth.get("keys")
        if isinstance(keys, list):
            for key in keys:
                k = str(key).strip()
                if k:
                    lines.append(f"auth_key={k}")

    health = data.get("health") or {}
    if isinstance(health, dict):
        if health.get("max_fails") is not None:
            lines.append(f"health_max_fails={int(health['max_fails'])}")
        ft = health.get("fail_timeout") or health.get("fail_timeout_sec")
        if ft is not None:
            s = str(ft).strip().rstrip("s")
            if s.isdigit():
                lines.append(f"health_fail_timeout_sec={int(s)}")
        active = health.get("active")
        if isinstance(active, dict) and active.get("path"):
            path = str(active["path"]).strip()
            if path.startswith("/"):
                lines.append("health_active=1")
                lines.append(f"health_active_path={path}")
                iv = active.get("interval") or active.get("interval_sec") or "5s"
                s = str(iv).strip().rstrip("s")
                if s.isdigit():
                    lines.append(f"health_active_interval_sec={int(s)}")

    limits = data.get("limits") or {}
    if limits.get("rate_limit_rps") is not None:
        lines.append(f"rate_limit_rps={int(limits['rate_limit_rps'])}")
    if limits.get("rate_limit_burst") is not None:
        lines.append(f"rate_limit_burst={int(limits['rate_limit_burst'])}")
    if limits.get("stream_idle_timeout") is not None:
        lines.append(f"stream_idle_timeout_sec={parse_duration(limits['stream_idle_timeout'], 'limits.stream_idle_timeout')}")
    if limits.get("stream_max_duration") is not None:
        lines.append(f"stream_max_duration_sec={parse_duration(limits['stream_max_duration'], 'limits.stream_max_duration')}")
    if limits.get("concurrent_streams") is not None:
        lines.append(f"concurrent_streams={int(limits['concurrent_streams'])}")

    routes = load_httpd_config(cfg_path)
    proxy_any = False
    for r in routes:
        kind = r.path_kind if r.path_kind in ("exact", "prefix", "prefix_strip") else "prefix"
        action = "proxy" if r.action.startswith("proxy:") else "static"
        if action == "proxy":
            proxy_any = True
        rps = int(getattr(r, "rate_limit_rps", 0) or 0)
        if rps > 0:
            burst = int(getattr(r, "rate_limit_burst", 0) or 0)
            if burst <= 0:
                burst = rps
            lines.append(f"route={r.method}|{r.path}|{kind}|{action}|{rps}|{burst}")
        else:
            lines.append(f"route={r.method}|{r.path}|{kind}|{action}")
        for req in getattr(r, "requires", []):
            lines.append(f"route_require={r.method}|{r.path}|{req}")

    pool_ports: dict[str, int] = {}
    nested = data.get("upstreams") or {}
    if isinstance(nested, dict):
        for pool_id, val in nested.items():
            if isinstance(val, dict):
                for peer in val.get("peers") or []:
                    p = peer_port(str(peer))
                    pool_ports[str(pool_id)] = p
                    lines.append(f"upstream_peer={p}")

    for key, val in data.items():
        if key.startswith("upstreams.") and isinstance(val, dict):
            pool_id = key.split(".", 1)[1]
            for peer in val.get("peers") or []:
                p = peer_port(str(peer))
                pool_ports[pool_id] = p
                lines.append(f"upstream_peer={p}")

    for _hdr, model, pool in validate_route_match(data):
        port = pool_ports.get(pool)
        if port is None:
            raise ValueError(f"route.match proxy {pool!r} has no peer port")
        lines.append(f"model_match={model}|{port}")

    if proxy_any and not any(l.startswith("upstream_peer=") for l in lines):
        lines.append("proxy_all=1")

    lc = data.get("leak_censor") or {}
    if isinstance(lc, dict) and leak_censor_enabled(data):
        lines.append("leak_censor_enabled=1")
        on_detect = str(lc.get("on_detect") or "redact")
        if on_detect in ("block_502", "abort_stream"):
            lines.append(f"leak_censor_on_detect={on_detect}")
        json_block = lc.get("json") if isinstance(lc.get("json"), dict) else data.get("leak_censor.json") or {}
        if not isinstance(json_block, dict):
            json_block = {}
        user_paths = json_block.get("deny_paths") or []
        include_gen = json_block.get("include_generated", False)
        include_gen = str(include_gen).lower() not in ("0", "false", "no")
        gen_paths, gen_hdrs = generated_paths_for_config(cfg_path.parent, include_gen)
        for p in list(dict.fromkeys([*(str(x) for x in user_paths), *gen_paths])):
            lines.append(f"leak_censor_deny_path={p}")
        pat_block = lc.get("patterns") if isinstance(lc.get("patterns"), dict) else data.get("leak_censor.patterns") or {}
        if isinstance(pat_block, dict):
            for pid in pat_block.get("allow") or []:
                if str(pid) in PATTERN_IDS:
                    lines.append(f"leak_censor_pattern={pid}")
        hdr_block = lc.get("headers") if isinstance(lc.get("headers"), dict) else data.get("leak_censor.headers") or {}
        if isinstance(hdr_block, dict):
            for name in hdr_block.get("deny_names") or []:
                lines.append(f"leak_censor_deny_header={name}")
        for name in gen_hdrs:
            lines.append(f"leak_censor_deny_header={name}")
    else:
        lines.append("leak_censor_enabled=0")

    try:
        lines.extend(tls_flatten_lines(data, cfg_path))
    except TlsError as e:
        raise ConfigError(str(e)) from e
    try:
        lines.extend(m2_flatten_lines(data, cfg_path))
    except M2Error as e:
        raise ConfigError(str(e)) from e
    try:
        lines.extend(m3_flatten_lines(data, cfg_path))
    except M3Error as e:
        raise ConfigError(str(e)) from e

    return lines


def main() -> int:
    p = argparse.ArgumentParser(description="flatten li-httpd.toml to runtime.conf")
    p.add_argument("config", type=Path)
    p.add_argument("-o", "--output", type=Path, required=True)
    args = p.parse_args()
    if not args.config.is_file():
        print(f"flatten-httpd-config: missing {args.config}", file=sys.stderr)
        return 1
    try:
        lines = flatten(args.config)
    except (ConfigError, M15Error, M2Error, TlsError, ValueError) as e:
        print(f"flatten-httpd-config: {e}", file=sys.stderr)
        return 1
    if not any(l.startswith("listen_port=") for l in lines):
        print("flatten-httpd-config: server.listen required", file=sys.stderr)
        return 1
    if not any(l.startswith("document_root=") for l in lines):
        print("flatten-httpd-config: server.document_root required", file=sys.stderr)
        return 1
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"flatten-httpd-config: wrote {args.output} ({len(lines)} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
