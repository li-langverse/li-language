"""TOML loading and merge for tier5_http — no scenario names here."""

from __future__ import annotations

import hashlib
import re
from copy import deepcopy
from pathlib import Path
from typing import Any

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib  # type: ignore

REPO = Path(__file__).resolve().parents[2]
TIER5 = REPO / "benchmarks" / "tier5_http"
SCENARIOS = TIER5 / "scenarios"


def load_toml(path: Path) -> dict[str, Any]:
    return tomllib.loads(path.read_text(encoding="utf-8"))


def deep_merge(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    out = deepcopy(base)
    for key, val in override.items():
        if key in out and isinstance(out[key], dict) and isinstance(val, dict):
            out[key] = deep_merge(out[key], val)
        else:
            out[key] = deepcopy(val)
    return out


def parse_set_kv(spec: str) -> tuple[str, Any]:
    """Parse dotted key path and scalar value (int, float, bool, str)."""
    if "=" not in spec:
        raise ValueError(f"--set requires key=value, got {spec!r}")
    key, raw = spec.split("=", 1)
    raw = raw.strip()
    if raw.lower() in ("true", "false"):
        return key, raw.lower() == "true"
    if re.fullmatch(r"-?\d+", raw):
        return key, int(raw)
    if re.fullmatch(r"-?\d+\.\d+", raw):
        return key, float(raw)
    return key, raw.strip('"').strip("'")


def apply_set(cfg: dict[str, Any], dotted: str, value: Any) -> None:
    parts = dotted.split(".")
    node = cfg
    for part in parts[:-1]:
        child = node.setdefault(part, {})
        if not isinstance(child, dict):
            raise ValueError(f"cannot set {dotted}: {part} is not a table")
        node = child
    node[parts[-1]] = value


def load_defaults() -> dict[str, Any]:
    return load_toml(TIER5 / "defaults.toml")


def load_suite() -> dict[str, Any]:
    return load_toml(TIER5 / "suite.toml")


def scenario_dir(name: str) -> Path:
    return SCENARIOS / name


def load_scenario_bench(name: str) -> dict[str, Any]:
    path = scenario_dir(name) / "bench.toml"
    if not path.is_file():
        raise FileNotFoundError(f"missing scenario bench.toml: {path}")
    data = load_toml(path)
    data.setdefault("name", name)
    return data


def merge_scenario(
    name: str,
    *,
    overrides: list[str] | None = None,
    profile: str | None = None,
) -> dict[str, Any]:
    defaults = load_defaults()
    bench = load_scenario_bench(name)
    merged = deep_merge(defaults, bench)
    merged["name"] = name
    if profile:
        merged["_profile"] = profile
    for spec in overrides or []:
        key, val = parse_set_kv(spec)
        apply_set(merged, key, val)
    return merged


def list_scenario_names(
    *,
    profile: str | None,
    explicit: list[str] | None,
) -> list[str]:
    """Resolve scenario list from suite.toml only (no hardcoded names)."""
    suite = load_suite()
    if explicit:
        return list(explicit)
    profiles = suite.get("profiles") or {}
    if profile and isinstance(profiles, dict) and profile in profiles:
        prof = profiles[profile]
        if isinstance(prof, dict) and prof.get("scenarios"):
            raw = prof["scenarios"]
            if isinstance(raw, list):
                return [str(x) for x in raw]
    default = suite.get("default") or {}
    if isinstance(default, dict):
        include = default.get("include") or []
        exclude = set(default.get("exclude") or [])
        if isinstance(include, list):
            return [str(x) for x in include if str(x) not in exclude]
    # Fallback: every scenarios/*/bench.toml with enabled=true
    names: list[str] = []
    if not SCENARIOS.is_dir():
        return names
    for child in sorted(SCENARIOS.iterdir()):
        bench_path = child / "bench.toml"
        if not bench_path.is_file():
            continue
        data = load_toml(bench_path)
        if data.get("enabled", True):
            names.append(child.name)
    return names


def profile_timing(profile: str | None) -> bool:
    if not profile:
        return True
    suite = load_suite()
    profiles = suite.get("profiles") or {}
    if not isinstance(profiles, dict):
        return True
    prof = profiles.get(profile)
    if isinstance(prof, dict) and "timing" in prof:
        return bool(prof["timing"])
    return True


def profile_langs(profile: str | None, merged: dict[str, Any]) -> list[str]:
    suite = load_suite()
    profiles = suite.get("profiles") or {}
    if profile and isinstance(profiles, dict):
        prof = profiles.get(profile)
        if isinstance(prof, dict) and prof.get("langs"):
            raw = prof["langs"]
            if isinstance(raw, list):
                return [str(x) for x in raw]
    server_langs = merged.get("langs")
    if isinstance(server_langs, list):
        return [str(x) for x in server_langs]
    return ["nginx", "li"]


def validate_merged(cfg: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if not cfg.get("name"):
        errors.append("missing name")
    if cfg.get("enabled") is False:
        return errors
    server = cfg.get("server")
    if not isinstance(server, dict):
        errors.append("missing [server] table")
    else:
        kind = server.get("kind")
        if kind not in ("static", "proxy", "li_config", "tcp_echo"):
            errors.append(f"unsupported server.kind={kind!r}")
    verify = cfg.get("verify")
    if verify is not None and not isinstance(verify, dict):
        errors.append("[verify] must be a table")
    load = cfg.get("load")
    if load is not None and not isinstance(load, dict):
        errors.append("[load] must be a table")
    return errors


def fixture_paths(cfg: dict[str, Any]) -> list[tuple[Path, int]]:
    """Return (relative_path, size_bytes) from defaults [fixtures] + scenario."""
    fixtures_tbl = cfg.get("fixtures") or {}
    defaults = load_defaults()
    base_fix = defaults.get("fixtures") or {}
    if not isinstance(base_fix, dict):
        base_fix = {}
    if not isinstance(fixtures_tbl, dict):
        fixtures_tbl = {}
    merged_fix: dict[str, Any] = {**base_fix, **fixtures_tbl}
    out: list[tuple[Path, int]] = []
    for key, val in merged_fix.items():
        if not isinstance(val, int) or val <= 0:
            continue
        rel = Path(str(key).replace("_", "/"))
        if not rel.suffix:
            rel = rel / "file.bin"
        out.append((rel, val))
    server = cfg.get("server") or {}
    if isinstance(server, dict) and server.get("kind") == "static":
        root = server.get("document_root")
        if isinstance(root, str) and "1k" in root:
            out.append((Path("static/1k/file.bin"), 1024))
    return out


def ensure_fixtures(cfg: dict[str, Any], tier5_root: Path | None = None) -> Path:
    root = tier5_root or TIER5
    fix_root = root / "fixtures"
    for rel, size in fixture_paths(cfg):
        path = fix_root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.stat().st_size != size:
            path.write_bytes(b"\x00" * size)
    return fix_root


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def resolve_document_root(cfg: dict[str, Any], tier5_root: Path | None = None) -> Path:
    root = tier5_root or TIER5
    server = cfg.get("server") or {}
    doc = server.get("document_root", "fixtures/static/1k")
    doc_path = Path(str(doc))
    if doc_path.is_absolute():
        return doc_path
    return (root / doc_path).resolve()


def render_nginx_conf(cfg: dict[str, Any], *, port: int, template_path: Path | None = None) -> str:
    tpl_path = template_path or (TIER5 / "templates" / "nginx.conf.in")
    tpl = tpl_path.read_text(encoding="utf-8")
    server = cfg.get("server") or {}
    global_tbl = cfg.get("global") or {}
    workers = global_tbl.get("workers", "auto")
    if workers == "auto":
        workers = "1"
    doc_root = resolve_document_root(cfg)
    sendfile = "on" if server.get("sendfile", True) else "off"
    return (
        tpl.replace("{{workers}}", str(workers))
        .replace("{{port}}", str(port))
        .replace("{{document_root}}", str(doc_root))
        .replace("{{sendfile}}", sendfile)
    )
