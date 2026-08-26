"""Start/stop tier5_http servers (nginx, li-httpd) — shared by bench_http and verify_http."""

from __future__ import annotations

import os
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any

from http_bench_toml import (
    REPO,
    TIER5,
    ensure_fixtures,
    merge_scenario,
    render_nginx_conf,
    resolve_document_root,
)

NGINX_CANDIDATES = (
    "nginx",
    "/usr/sbin/nginx",
    "/usr/local/sbin/nginx",
)


def resolve_nginx_binary() -> str | None:
    for cand in NGINX_CANDIDATES:
        if Path(cand).is_file() and os.access(cand, os.X_OK):
            return cand
    found = shutil.which("nginx")
    return found if found else None


def scenario_port(cfg: dict[str, Any], name: str) -> int:
    """Stable listen port per scenario (not Python hash())."""
    global_tbl = cfg.get("global") or {}
    port_base = int(global_tbl.get("port_base", 18080))
    offset = sum(ord(c) for c in name) % 200
    return port_base + offset


def pick_free_port(base: int, offset: int, *, attempts: int = 40) -> int:
    for i in range(attempts):
        port = base + ((offset + i * 17) % 400)
        if port < 1024 or port > 65000:
            continue
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.bind(("127.0.0.1", port))
                return port
        except OSError:
            continue
    return base + (offset % 200)


def wait_port(port: int, timeout: float = 10.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def stop_proc(proc: subprocess.Popen[object] | None) -> None:
    if proc is None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()


def nginx_error_text(prefix: Path, stderr: str) -> str:
    err = stderr.strip()
    elog = prefix / "logs" / "error.log"
    if elog.is_file():
        tail = elog.read_text(encoding="utf-8", errors="replace").strip()[-800:]
        if tail:
            err = f"{err}\n{tail}" if err else tail
    return err or "unknown nginx error"


def start_nginx(cfg: dict[str, Any], port: int) -> tuple[subprocess.Popen[object] | None, Path | None]:
    nginx = resolve_nginx_binary()
    if not nginx:
        return None, None
    conf_dir = Path(tempfile.mkdtemp(prefix="tier5_bench_"))
    conf_path = conf_dir / "nginx.conf"
    prefix = conf_dir / "prefix"
    conf_path.write_text(render_nginx_conf(cfg, port=port), encoding="utf-8")
    prefix.mkdir(parents=True, exist_ok=True)
    (prefix / "logs").mkdir(exist_ok=True)
    proc = subprocess.Popen(
        [nginx, "-g", "daemon off;", "-c", str(conf_path), "-p", str(prefix)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    time.sleep(0.35)
    if not wait_port(port, timeout=5.0):
        err = proc.stderr.read() if proc.stderr else ""
        stop_proc(proc)
        shutil.rmtree(conf_dir, ignore_errors=True)
        raise RuntimeError(f"nginx failed to start: {nginx_error_text(prefix, err)}")
    if proc.poll() is not None:
        err = proc.stderr.read() if proc.stderr else ""
        shutil.rmtree(conf_dir, ignore_errors=True)
        raise RuntimeError(f"nginx exited early: {nginx_error_text(prefix, err)}")
    return proc, conf_dir


def prepare_li_httpd_conf(name: str, port: int) -> tuple[Path, Path]:
    merged = merge_scenario(name, profile="ci", overrides=None)
    ensure_fixtures(merged)
    doc_root = resolve_document_root(merged)
    work = Path(tempfile.mkdtemp(prefix="tier5_bench_li_"))
    conf = work / "runtime.conf"
    conf.write_text(f"listen_port={port}\ndocument_root={doc_root}\n", encoding="utf-8")
    return conf, work


def start_li_httpd(name: str, port: int) -> tuple[subprocess.Popen[object] | None, Path | None]:
    bin_path = os.environ.get("LI_HTTPD_BIN", str(REPO / "build" / "li-httpd"))
    if not os.path.isfile(bin_path) or not os.access(bin_path, os.X_OK):
        return None, None
    conf_path, work_dir = prepare_li_httpd_conf(name, port)
    env = os.environ.copy()
    env.setdefault("LI_HTTPD_ACCESS_LOG", "0")
    proc = subprocess.Popen(
        [bin_path, str(conf_path)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    time.sleep(0.45)
    if proc.poll() is not None:
        err = proc.stderr.read() if proc.stderr else ""
        shutil.rmtree(work_dir, ignore_errors=True)
        raise RuntimeError(f"li-httpd failed to start: {err.strip() or 'unknown'}")
    if not wait_port(port, timeout=8.0):
        stop_proc(proc)
        shutil.rmtree(work_dir, ignore_errors=True)
        raise RuntimeError(f"li-httpd did not listen on {port}")
    return proc, work_dir


def bench_scenario_dir(name: str) -> Path:
    return TIER5 / "scenarios" / name
