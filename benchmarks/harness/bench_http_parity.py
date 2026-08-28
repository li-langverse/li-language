"""Nginx vs li-httpd ratio checks for tier5_http agent-gateway scenarios."""

from __future__ import annotations

import os
from typing import Any


def metric_rows(rows: list[dict[str, object]], *, benchmark: str, lang: str, metric: str) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    for row in rows:
        if row.get("benchmark") != benchmark:
            continue
        if row.get("lang") != lang:
            continue
        if row.get("metric") != metric:
            continue
        out.append(row)
    return out


def best_metric(rows: list[dict[str, object]], *, higher_better: bool) -> float | None:
    vals: list[float] = []
    for row in rows:
        try:
            vals.append(float(row["value"]))
        except (TypeError, ValueError, KeyError):
            continue
    if not vals:
        return None
    return max(vals) if higher_better else min(vals)


def check_parity_ratios(
    rows: list[dict[str, object]],
    scenarios: list[str],
    *,
    rps_min: float | None = None,
    p99_max: float | None = None,
) -> tuple[bool, list[str]]:
    """li must meet or beat nginx: RPS ratio >= rps_min, p99 ratio <= p99_max."""
    rps_min = float(os.environ.get("HTTPD_BENCH_RPS_RATIO_MIN", rps_min if rps_min is not None else "0.95"))
    p99_max = float(os.environ.get("HTTPD_BENCH_P99_RATIO_MAX", p99_max if p99_max is not None else "2.0"))
    notes: list[str] = []
    ok_all = True
    for name in scenarios:
        nginx_rps = best_metric(
            metric_rows(rows, benchmark=name, lang="nginx", metric="rps"), higher_better=True
        )
        li_rps = best_metric(metric_rows(rows, benchmark=name, lang="li", metric="rps"), higher_better=True)
        if nginx_rps and li_rps:
            ratio = li_rps / nginx_rps
            line = f"{name}: rps li/nginx={ratio:.3f} (li={li_rps:.0f} nginx={nginx_rps:.0f})"
            notes.append(line)
            if ratio < rps_min:
                ok_all = False
                notes.append(f"  FAIL rps ratio < {rps_min}")
        nginx_p99 = best_metric(
            metric_rows(rows, benchmark=name, lang="nginx", metric="p99_latency_ms"),
            higher_better=False,
        )
        li_p99 = best_metric(
            metric_rows(rows, benchmark=name, lang="li", metric="p99_latency_ms"),
            higher_better=False,
        )
        if nginx_p99 and li_p99 and nginx_p99 > 0:
            ratio = li_p99 / nginx_p99
            line = f"{name}: p99 li/nginx={ratio:.3f} (li={li_p99:.2f}ms nginx={nginx_p99:.2f}ms)"
            notes.append(line)
            if ratio > p99_max:
                ok_all = False
                notes.append(f"  FAIL p99 ratio > {p99_max}")
    return ok_all, notes
