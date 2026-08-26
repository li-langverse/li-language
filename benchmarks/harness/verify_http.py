#!/usr/bin/env python3
"""Correctness gate for tier5_http — reads [verify] from merged bench.toml only."""

from __future__ import annotations

import argparse
import csv
import functools
import hashlib
import http.client
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from contextlib import closing
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from http_bench_servers import (
    pick_free_port,
    resolve_nginx_binary,
    start_li_httpd,
    start_nginx,
    stop_proc,
    wait_port,
)
from http_bench_toml import (
    REPO,
    ensure_fixtures,
    list_scenario_names,
    merge_scenario,
    profile_langs,
    resolve_document_root,
    validate_merged,
)

RESULTS = REPO / "benchmarks" / "results"


def _http_request(host: str, port: int, method: str, path: str, timeout: float = 5.0) -> tuple[int, bytes]:
    conn = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        conn.request(method, path)
        resp = conn.getresponse()
        body = resp.read()
        return resp.status, body
    finally:
        conn.close()


def _run_verify_requests(
    cfg: dict[str, Any],
    *,
    host: str,
    port: int,
    doc_root: Path,
) -> tuple[bool, str]:
    verify = cfg.get("verify") or {}
    requests = verify.get("requests") or []
    if not isinstance(requests, list) or not requests:
        return True, "no [verify].requests (ok)"
    for item in requests:
        if not isinstance(item, dict):
            return False, "verify.requests entry must be a table"
        method = str(item.get("method", "GET")).upper()
        path = str(item.get("path", "/"))
        expect_status = int(item.get("expect_status", 200))
        status, body = _http_request(host, port, method, path)
        if status != expect_status:
            return False, f"{method} {path}: status {status} != {expect_status}"
        expect_sha = item.get("body_sha256")
        if expect_sha:
            got = hashlib.sha256(body).hexdigest()
            if str(expect_sha).lower() != got.lower():
                return False, f"{method} {path}: body sha256 mismatch"
        expect_len = item.get("body_len")
        if expect_len is not None and len(body) != int(expect_len):
            return False, f"{method} {path}: body len {len(body)} != {expect_len}"
        expect_sub = item.get("body_contains")
        if expect_sub and str(expect_sub).encode() not in body:
            return False, f"{method} {path}: body missing {expect_sub!r}"
    rel_root = doc_root
    _ = rel_root  # reserved for future path checks
    return True, f"verify ok ({len(requests)} requests)"


class _QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        return


def _start_stub_static(doc_root: Path, port: int) -> ThreadingHTTPServer:
    handler = functools.partial(_QuietHandler, directory=str(doc_root))
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
    import threading

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server


def verify_scenario(
    name: str,
    *,
    profile: str | None,
    overrides: list[str] | None,
    use_stub: bool,
) -> tuple[bool, str]:
    cfg = merge_scenario(name, overrides=overrides, profile=profile)
    errs = validate_merged(cfg)
    if errs:
        return False, "; ".join(errs)
    if cfg.get("enabled") is False:
        return True, "disabled"
    ensure_fixtures(cfg)
    doc_root = resolve_document_root(cfg)
    if not doc_root.is_dir():
        return False, f"document_root missing: {doc_root}"

    port_base = int((cfg.get("global") or {}).get("port_base", 18080))
    port = pick_free_port(port_base, sum(ord(c) for c in name) % 200)

    langs = profile_langs(profile, cfg)
    if use_stub:
        langs = ["stub"]

    last_msg = ""
    for lang in langs:
        proc: subprocess.Popen[object] | None = None
        server: ThreadingHTTPServer | None = None
        work_dir: Path | None = None
        try:
            if lang == "nginx" and resolve_nginx_binary():
                proc, work_dir = start_nginx(cfg, port)
            elif lang == "li":
                proc, work_dir = start_li_httpd(name, port)
            else:
                server = _start_stub_static(doc_root, port)
                if not wait_port(port):
                    return False, f"stub server did not listen on {port}"
            if proc is not None and not wait_port(port):
                return False, f"{lang} did not listen on {port}"
            ok, msg = _run_verify_requests(cfg, host="127.0.0.1", port=port, doc_root=doc_root)
            last_msg = f"{lang}: {msg}"
            if not ok:
                return False, last_msg
        except RuntimeError as exc:
            return False, str(exc)
        finally:
            stop_proc(proc)
            if server is not None:
                server.shutdown()
            if work_dir and work_dir.exists():
                shutil.rmtree(work_dir, ignore_errors=True)
    return True, last_msg or "ok"


def main() -> int:
    parser = argparse.ArgumentParser(description="tier5_http verify (TOML-driven)")
    parser.add_argument("scenarios", nargs="*", help="scenario names (default: suite profile)")
    parser.add_argument("--profile", default="ci", help="suite.toml profile")
    parser.add_argument("--set", action="append", default=[], dest="overrides")
    parser.add_argument("--write-csv", type=Path, default=RESULTS / "verify_http.csv")
    parser.add_argument(
        "--stub",
        action="store_true",
        help="use Python static stub instead of nginx (default when nginx missing)",
    )
    args = parser.parse_args()

    names = list_scenario_names(profile=args.profile, explicit=args.scenarios or None)
    if not names:
        print("verify_http: no scenarios resolved from suite.toml", file=sys.stderr)
        return 1

    use_stub = args.stub or not resolve_nginx_binary()
    if os.environ.get("TIER5_HTTP_STUB", "").strip() in ("1", "true", "yes"):
        use_stub = True

    rows: list[list[object]] = []
    ok_all = True
    for name in names:
        cfg = merge_scenario(name, overrides=args.overrides, profile=args.profile)
        if cfg.get("enabled") is False:
            print(f"SKIP {name} (disabled)")
            rows.append([name, "—", "skip", 0, "enabled", 0, "bool", "", "", "", True])
            continue
        try:
            ok, msg = verify_scenario(
                name,
                profile=args.profile,
                overrides=args.overrides,
                use_stub=use_stub,
            )
        except Exception as exc:
            ok, msg = False, str(exc)
        ok_all = ok_all and ok
        status = "PASS" if ok else "FAIL"
        print(f"{status} verify_http {name}: {msg}")
        rows.append(
            [
                name,
                "stub" if use_stub else "nginx",
                "verify",
                1,
                "requests",
                1 if ok else 0,
                "bool",
                "",
                "",
                "",
                ok,
            ]
        )

    args.write_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.write_csv.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "benchmark",
                "lang",
                "variant",
                "threads",
                "metric",
                "value",
                "unit",
                "git_sha",
                "cpu_model",
                "flags",
                "passed",
            ]
        )
        w.writerows(rows)
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
