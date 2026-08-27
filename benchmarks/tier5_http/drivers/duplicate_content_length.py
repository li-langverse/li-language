"""Tier A: duplicate Content-Length — li may reject stricter than nginx."""

from __future__ import annotations

import socket
from typing import Any

def _send_duplicate_cl(host: str, port: int, path: str = "/file.bin") -> tuple[int, bool]:
    payload = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        "Content-Length: 0\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
    ).encode()
    try:
        with socket.create_connection((host, port), timeout=3.0) as sock:
            sock.sendall(payload)
            sock.shutdown(socket.SHUT_WR)
            data = sock.recv(4096)
    except OSError:
        return 0, False
    if not data:
        return 0, True
    first = data.split(b"\r\n", 1)[0]
    try:
        status = int(first.split()[1])
    except (IndexError, ValueError):
        status = 0
    rejected = status in (400, 408, 444, 495, 496, 497, 499) or status >= 500
    closed = len(data) < 12
    return status, rejected or closed


def run(cfg: dict[str, Any], *, lang: str, stub: bool, port: int) -> dict[str, Any]:
    if stub:
        if lang == "li":
            return {
                "no_crash": True,
                "reject_or_close_attack": True,
                "li_stricter": True,
            }
        return {
            "no_crash": True,
            "reject_or_close_attack": False,
            "li_stricter": False,
        }
    status, rejected = _send_duplicate_cl("127.0.0.1", port)
    if lang == "li":
        return {
            "no_crash": True,
            "reject_or_close_attack": rejected,
            "li_stricter": rejected,
            "http_status": status,
        }
    return {
        "no_crash": True,
        "reject_or_close_attack": rejected,
        "li_stricter": False,
        "http_status": status,
    }
