#!/usr/bin/env python3
"""Easy li-httpd TOML desugar + validate (M1 prep — no Li binary required)."""

from __future__ import annotations

import re
import sys
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any



class ConfigError(Exception):
    pass


ROUTE_KEY_RE = re.compile(
    r"^(?P<method>[A-Z]+)\s+(?P<path>/[^\s#]+)(?:\s+(?P<extras>.+))?$"
)
HEADER_EXTRA_RE = re.compile(r"^([a-zA-Z0-9_-]+)=([^\s]+)$")

# M1 ingress allowlist (plan § header controls — route-key extras are client→gateway hints)
INGRESS_HEADER_ALLOW = frozenset(
    {
        "authorization",
        "content-type",
        "accept",
        "traceparent",
        "x-request-id",
        "x-agent-id",
        "x-model",
        "idempotency-key",
    }
)
HOP_BY_HOP_HEADERS = frozenset(
    {
        "connection",
        "keep-alive",
        "proxy-authenticate",
        "proxy-authorization",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
    }
)


def validate_ingress_header_name(name: str) -> None:
    n = name.lower().strip()
    if n in HOP_BY_HOP_HEADERS:
        raise ConfigError(f"hop-by-hop header not allowed in route extras: {name!r}")
    if n.startswith("x-upstream-") or n.startswith("x-route-"):
        raise ConfigError(f"forbidden header prefix in route extras: {name!r}")
    if n not in INGRESS_HEADER_ALLOW:
        raise ConfigError(
            f"header {name!r} not in ingress allowlist (M1: {sorted(INGRESS_HEADER_ALLOW)})"
        )


ROUTE_REQUIRE_ALLOW = frozenset({"traceparent", "websocket"})


@dataclass
class CanonicalRoute:
    name: str
    method: str
    path: str
    path_kind: str  # exact | prefix | prefix_strip
    action: str
    headers: dict[str, str]
    priority: int
    requires: list[str] = field(default_factory=list)


def slug_route_name(method: str, path: str) -> str:
    p = path
    if p.endswith("/**"):
        p = p[:-3] + "_rest"
    elif p.endswith("/*"):
        p = p[:-2] + "_wild"
    s = f"{method.lower()}_{p.strip('/')}".replace("/", "_").replace("*", "wild")
    s = re.sub(r"[^a-z0-9_]+", "_", s).strip("_")
    return s or "route"


def parse_path_kind(path: str) -> tuple[str, str]:
    if path.endswith("/**"):
        return path[:-3], "prefix_strip"
    if path.endswith("/*"):
        return path[:-2], "prefix"
    return path, "exact"


def parse_route_key(key: str, action: str, priority: int) -> CanonicalRoute:
    m = ROUTE_KEY_RE.match(key.strip())
    if not m:
        raise ConfigError(f"invalid route key: {key!r}")
    method = m.group("method")
    raw_path = m.group("path")
    extras = (m.group("extras") or "").strip()
    headers: dict[str, str] = {}
    requires: list[str] = []
    if extras:
        for part in extras.split():
            req_m = re.match(r"^require=([a-z0-9_-]+)$", part)
            if req_m:
                req_name = req_m.group(1).lower()
                if req_name not in ROUTE_REQUIRE_ALLOW:
                    raise ConfigError(
                        f"unsupported require={req_name!r} in {key!r} (allowed: {sorted(ROUTE_REQUIRE_ALLOW)})"
                    )
                requires.append(req_name)
                continue
            hm = HEADER_EXTRA_RE.match(part)
            if not hm:
                raise ConfigError(f"invalid route extra: {part!r} in {key!r}")
            hname = hm.group(1).lower()
            validate_ingress_header_name(hname)
            headers[hname] = hm.group(2)
    if ".." in raw_path or "//" in raw_path.replace("://", ""):
        raise ConfigError(f"path must not contain .. or //: {raw_path}")
    norm_path, kind = parse_path_kind(raw_path)
    return CanonicalRoute(
        name=slug_route_name(method, raw_path),
        method=method,
        path=norm_path,
        path_kind=kind,
        action=str(action).strip().strip('"'),
        headers=headers,
        priority=priority,
        requires=requires,
    )


def parse_canonical_routes(rows: list[Any]) -> list[CanonicalRoute]:
    out: list[CanonicalRoute] = []
    for row in rows:
        if not isinstance(row, dict):
            raise ConfigError("[[routes]] entry must be a table")
        method = str(row.get("method", "")).strip()
        path = str(row.get("path", "")).strip()
        action = str(row.get("action", "")).strip()
        if not method or not path or not action:
            raise ConfigError("[[routes]] requires method, path, action")
        if ".." in path or "//" in path.replace("://", ""):
            raise ConfigError(f"path must not contain .. or //: {path}")
        kind = str(row.get("path_kind", "exact")).strip()
        if kind not in ("exact", "prefix", "prefix_strip"):
            raise ConfigError(f"invalid path_kind: {kind!r}")
        row_headers = row.get("headers") or {}
        if isinstance(row_headers, dict):
            for hk, hv in row_headers.items():
                validate_ingress_header_name(str(hk))
                if not str(hv).strip():
                    raise ConfigError(f"empty header value for {hk!r}")
        out.append(
            CanonicalRoute(
                name=str(row.get("name") or slug_route_name(method, path)),
                method=method,
                path=path,
                path_kind=kind,
                action=action,
                headers={
                    str(k).lower(): str(v)
                    for k, v in (row_headers.items() if isinstance(row_headers, dict) else [])
                },
                priority=int(row.get("priority", 0)),
            )
        )
    return out


def desugar_config(data: dict[str, Any]) -> list[CanonicalRoute]:
    routes_tbl = data.get("routes")
    if routes_tbl is None:
        return []
    if isinstance(routes_tbl, list):
        return parse_canonical_routes(routes_tbl)
    if not isinstance(routes_tbl, dict):
        raise ConfigError("[routes] must be a table (map) or [[routes]] array")
    out: list[CanonicalRoute] = []
    for i, (key, action) in enumerate(routes_tbl.items()):
        out.append(parse_route_key(str(key), str(action), priority=i))
    return out


def routes_overlap(a: CanonicalRoute, b: CanonicalRoute) -> bool:
    if a.method != b.method and a.method != "*" and b.method != "*":
        return False
    pa, pb = a.path.rstrip("/"), b.path.rstrip("/")
    if pa == pb:
        return True
    if pa.startswith(pb + "/") or pb.startswith(pa + "/"):
        return True
    return False


def validate_routes(routes: list[CanonicalRoute]) -> None:
    for i, a in enumerate(routes):
        for b in routes[i + 1 :]:
            if routes_overlap(a, b) and a.priority != b.priority:
                continue
            if routes_overlap(a, b):
                raise ConfigError(
                    f"overlapping routes at same priority: {a.name} vs {b.name}"
                )


def validate_rate_limits_cfg(data: dict[str, Any]) -> None:
    """Require limits.rate_limit_rps when any proxy: route exists (M1 agent gateway)."""
    limits = data.get("limits") or {}
    routes = data.get("routes")
    has_proxy = isinstance(routes, dict) and any(
        isinstance(a, str) and a.strip().startswith("proxy:") for a in routes.values()
    )
    if not has_proxy:
        return
    rps = limits.get("rate_limit_rps")
    if rps is None:
        raise ConfigError(
            "limits.rate_limit_rps is required when routes include proxy: (M1 public/agent gate)"
        )
    try:
        n = int(rps)
    except (TypeError, ValueError) as e:
        raise ConfigError("limits.rate_limit_rps must be a positive integer") from e
    if n < 1 or n > 100_000:
        raise ConfigError("limits.rate_limit_rps must be in [1, 100000]")
    burst = limits.get("rate_limit_burst")
    if burst is not None:
        try:
            b = int(burst)
        except (TypeError, ValueError) as e:
            raise ConfigError("limits.rate_limit_burst must be a positive integer") from e
        if b < n:
            raise ConfigError("limits.rate_limit_burst must be >= limits.rate_limit_rps")


def load_httpd_config(path: Path) -> list[CanonicalRoute]:
    from httpd_leak_censor import ConfigError as LeakError
    from httpd_leak_censor import validate_leak_censor
    from httpd_m15 import ConfigError as M15Error
    from httpd_m15 import validate_inference_require, validate_m15_limits, validate_route_match
    from httpd_m2 import ConfigError as M2Error
    from httpd_m2 import validate_m2_config
    from httpd_m3 import ConfigError as M3Error
    from httpd_m3 import validate_m3_config
    from httpd_rng import ConfigError as RngError
    from httpd_rng import validate_rng_config_raise
    from httpd_tls import ConfigError as TlsError
    from httpd_tls import validate_tls_config

    data = tomllib.loads(path.read_text(encoding="utf-8"))
    validate_rate_limits_cfg(data)
    try:
        validate_m15_limits(data)
        validate_route_match(data)
        validate_inference_require(data)
        for warn in validate_leak_censor(data, path):
            print(f"warning: {warn}", file=sys.stderr)
        validate_tls_config(data, path)
        validate_m2_config(data, path)
        validate_m3_config(data, path)
        for warn in validate_rng_config_raise(data):
            print(f"warning: {warn}", file=sys.stderr)
    except (M15Error, LeakError, TlsError, M2Error, M3Error, RngError) as e:
        raise ConfigError(str(e)) from e
    routes = desugar_config(data)
    validate_routes(routes)
    return routes


def explain(routes: list[CanonicalRoute]) -> str:
    lines = ["# canonical routes (desugared)"]
    for r in routes:
        parts = [f"{k}={v}" for k, v in sorted(r.headers.items())]
        for req in sorted(r.requires):
            parts.append(f"require={req}")
        hdr = " ".join(parts)
        extra = f" [{hdr}]" if hdr else ""
        lines.append(
            f"[[routes]]\n"
            f'name = "{r.name}"\n'
            f"priority = {r.priority}\n"
            f'method = "{r.method}"\n'
            f'path = "{r.path}"\n'
            f'path_kind = "{r.path_kind}"\n'
            f'action = "{r.action}"{extra}\n'
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: httpd_config.py <config.toml> [--explain]", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    routes = load_httpd_config(path)
    if "--explain" in sys.argv:
        print(explain(routes), end="")
    else:
        print(f"OK: {len(routes)} routes")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        # diff/head may close stdout early (explain-config golden checks).
        sys.exit(0)
    except ConfigError as e:
        print(f"config error: {e}", file=sys.stderr)
        sys.exit(1)
