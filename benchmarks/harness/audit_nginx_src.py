#!/usr/bin/env python3
"""Read-only nginx source audit — emit/validate nginx_mitigations.toml checklist.

Scans nginx CHANGES (submodule or fixtures/) for Security/CVE lines and validates
the curated mitigation table. Does not track Li implementation status (no li_done).
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path
from typing import Any

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib  # type: ignore

from http_bench_toml import REPO, TIER5

NGINX_ROOT = TIER5 / "third_party" / "nginx"
MITIGATIONS_PATH = TIER5 / "nginx_mitigations.toml"
EXPLOITS_DIR = TIER5 / "exploits"
CHANGES_CANDIDATES = (
    NGINX_ROOT / "CHANGES",
    TIER5 / "fixtures" / "nginx-1.26.2-CHANGES.txt",
)
NGINX_PIN_TAG = "release-1.26.2"
NGINX_PIN_VERSION = "1.26.2"

REQUIRED_MITIGATION_KEYS = ("id", "src", "notes", "li_invariant")
CVE_RE = re.compile(r"CVE-\d{4}-\d+", re.IGNORECASE)
SECURITY_RE = re.compile(r"\*\)\s*Security:", re.IGNORECASE)
# nginx C tree only — Li-only rows use docs/, scripts/, li-tests/ under REPO.
NGINX_SRC_PREFIX = "src/"

# Curated keyword → default src hint when matching CHANGES text to checklist ids.
KEYWORD_SRC: dict[str, tuple[str, str]] = {
    "duplicate_content_length": (
        "src/http/ngx_http_parse.c",
        "ngx_http_parse_header_line",
    ),
    "client_header_timeout": (
        "src/http/ngx_http_request.c",
        "ngx_http_process_request_header",
    ),
    "large_client_header_buffers": (
        "src/http/ngx_http_request.c",
        "ngx_http_process_request_line",
    ),
    "client_max_body_size": (
        "src/http/ngx_http_request_body.c",
        "ngx_http_read_client_request_body",
    ),
    "request_smuggling_cl_te": (
        "src/http/ngx_http_parse.c",
        "ngx_http_parse_header_line",
    ),
    "proxy_duplicate_headers": (
        "src/http/ngx_http_upstream.c",
        "ngx_http_upstream_process_headers",
    ),
    "h2_rapid_reset": (
        "src/http/v2/ngx_http_v2.c",
        "ngx_http_v2_read_handler",
    ),
    "dns_resolver_limits": (
        "src/core/ngx_resolver.c",
        "ngx_resolver_create_name_query",
    ),
}


def load_changes_text() -> tuple[str, Path]:
    for path in CHANGES_CANDIDATES:
        if path.is_file():
            return path.read_text(encoding="utf-8", errors="replace"), path
    raise FileNotFoundError(
        "nginx CHANGES not found; init submodule or keep fixtures/nginx-1.26.2-CHANGES.txt"
    )


def changes_sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def parse_cves_from_changes(text: str) -> set[str]:
    return {m.group(0).upper() for m in CVE_RE.finditer(text)}


def parse_security_bullets(text: str) -> list[str]:
    bullets: list[str] = []
    for line in text.splitlines():
        if SECURITY_RE.search(line):
            bullets.append(line.strip())
    return bullets


def load_mitigations() -> dict[str, Any]:
    return tomllib.loads(MITIGATIONS_PATH.read_text(encoding="utf-8"))


def mitigation_rows(data: dict[str, Any]) -> list[dict[str, Any]]:
    rows = data.get("mitigation") or []
    return [r for r in rows if isinstance(r, dict)]


def validate_mitigations(
    *,
    rows: list[dict[str, Any]],
    changes_cves: set[str],
    nginx_src_ok: bool,
    strict_cve: bool,
) -> list[str]:
    errs: list[str] = []
    seen: set[str] = set()
    for row in rows:
        mid = str(row.get("id", "")).strip()
        if not mid:
            errs.append("mitigation row missing id")
            continue
        if mid in seen:
            errs.append(f"duplicate mitigation id: {mid}")
        seen.add(mid)
        for key in REQUIRED_MITIGATION_KEYS:
            if not str(row.get(key, "")).strip():
                errs.append(f"{mid}: missing required field {key}")
        if "li_done" in row:
            errs.append(f"{mid}: li_done is not allowed (read-only checklist)")
        src = str(row.get("src", "")).strip()
        if src:
            if src.startswith(NGINX_SRC_PREFIX):
                if nginx_src_ok and not (NGINX_ROOT / src).is_file():
                    errs.append(f"{mid}: src path missing in submodule: {src}")
            elif not (REPO / src).is_file():
                errs.append(f"{mid}: src path missing in repo: {src}")
        exploit = str(row.get("exploit", "")).strip()
        if exploit:
            path = TIER5 / exploit
            if not path.is_file():
                errs.append(f"{mid}: exploit TOML missing: {exploit}")
        if strict_cve:
            cves = row.get("cve") or []
            if isinstance(cves, list):
                for cve in cves:
                    c = str(cve).upper()
                    if c and c not in changes_cves:
                        errs.append(f"{mid}: CVE {c} not found in CHANGES digest")
    return errs


def audit_report() -> int:
    changes_text, changes_path = load_changes_text()
    cves = parse_cves_from_changes(changes_text)
    bullets = parse_security_bullets(changes_text)
    nginx_src_ok = (NGINX_ROOT / "src" / "http").is_dir()
    data = load_mitigations()
    rows = mitigation_rows(data)
    meta = data.get("audit") or {}
    print(f"nginx audit: CHANGES={changes_path.name} sha256={changes_sha256(changes_text)[:16]}…")
    print(f"  Security bullets: {len(bullets)}  CVE tokens: {len(cves)}  checklist rows: {len(rows)}")
    print(f"  Submodule src tree: {'yes' if nginx_src_ok else 'no (fixture-only)'}")
    if isinstance(meta, dict):
        pin = meta.get("nginx_tag", NGINX_PIN_TAG)
        print(f"  Pinned tag (metadata): {pin}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Validate nginx_mitigations.toml against CHANGES and paths",
    )
    parser.add_argument(
        "--report",
        action="store_true",
        help="Print audit summary (default if no flags)",
    )
    parser.add_argument(
        "--strict-cve",
        action="store_true",
        help="Require every listed CVE to appear in CHANGES (default: off)",
    )
    args = parser.parse_args()
    if not MITIGATIONS_PATH.is_file():
        print(f"audit_nginx_src: missing {MITIGATIONS_PATH}", file=sys.stderr)
        return 1
    if args.check:
        changes_text, _ = load_changes_text()
        cves = parse_cves_from_changes(changes_text)
        nginx_src_ok = (NGINX_ROOT / "src" / "http").is_dir()
        rows = mitigation_rows(load_mitigations())
        errs = validate_mitigations(
            rows=rows,
            changes_cves=cves,
            nginx_src_ok=nginx_src_ok,
            strict_cve=args.strict_cve,
        )
        if errs:
            for e in errs:
                print(f"audit_nginx_src: {e}", file=sys.stderr)
            return 1
        print(f"audit_nginx_src: OK ({len(rows)} mitigations, {len(cves)} CVEs in CHANGES)")
        return 0
    return audit_report()


if __name__ == "__main__":
    sys.exit(main())
