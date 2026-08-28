"""Normative float64 references for tier-1 micro benchmarks.

Correctness means matching these specs (init + reduction order from tier1 ``common/*_core.c``),
not matching another language runtime. Python evaluates the same loops in float64.

Compiler DCE and fast-math folds are **allowed**; a build is valid only after **our** checks
(spec, small/large goldens, magnitude floors, optional min Li wall time) pass.

Optional ``decimal`` high-precision checks are used only to validate the spec on small sizes.
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass
from typing import Callable


def format_result(value: float) -> str:
    """Match benchmark ``printf(\"%.17g\\n\", ...)``."""
    if math.isinf(value):
        return "inf" if value > 0 else "-inf"
    if math.isnan(value):
        return "nan"
    return f"{value:.17g}"


def parse_result(text: str) -> float:
    t = text.strip().lower()
    if t == "inf":
        return math.inf
    if t == "-inf":
        return -math.inf
    if t == "nan":
        return math.nan
    return float(text.strip())


def float_close(actual: float, expected: float, *, rtol: float, atol: float = 0.0) -> bool:
    if math.isnan(actual) or math.isnan(expected):
        return False
    if math.isinf(actual) or math.isinf(expected):
        return actual == expected
    scale = max(abs(expected), abs(actual), 1.0)
    return abs(actual - expected) <= atol + rtol * scale


# --- kernel specs (normative) -------------------------------------------------


def dot_spec_sum(n: int) -> float:
    """``dot_core.c``: fill ``a[i], b[i]`` then sum ``a[i]*b[i]``."""
    a = [float(i & 255) * 0.001 for i in range(n)]
    b = [float((i * 7) & 255) * 0.002 for i in range(n)]
    acc = 0.0
    for i in range(n):
        acc += a[i] * b[i]
    return acc


def matmul_naive_spec_sum(n: int) -> float:
    """``matmul_core.c``: i-k-j multiply, sum all ``c[i][j]``."""
    a = [[float((i + j) % 17) * 0.01 for j in range(n)] for i in range(n)]
    b = [[float((i * 3 + j) % 13) * 0.02 for j in range(n)] for i in range(n)]
    c = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            aik = a[i][k]
            for j in range(n):
                c[i][j] += aik * b[k][j]
    acc = 0.0
    for i in range(n):
        for j in range(n):
            acc += c[i][j]
    return acc


def matmul_blocked_spec_sum(n: int, *, block: int = 64) -> float:
    """``matmul_blocked_core.c``: tiled i-k-j, sum all ``c[i][j]``."""
    a = [[float((i + j) % 17) * 0.01 for j in range(n)] for i in range(n)]
    b = [[float((i * 3 + j) % 13) * 0.02 for j in range(n)] for i in range(n)]
    c = [[0.0] * n for _ in range(n)]
    for ii in range(0, n, block):
        for kk in range(0, n, block):
            for jj in range(0, n, block):
                i_max = min(ii + block, n)
                k_max = min(kk + block, n)
                j_max = min(jj + block, n)
                for i in range(ii, i_max):
                    for k in range(kk, k_max):
                        aik = a[i][k]
                        for j in range(jj, j_max):
                            c[i][j] += aik * b[k][j]
    acc = 0.0
    for i in range(n):
        for j in range(n):
            acc += c[i][j]
    return acc


def reduce_sum_spec(n: int) -> float:
    """``reduce_core.c``: sum ``(i & 1023) * 1e-6``."""
    acc = 0.0
    for i in range(n):
        acc += float(i & 1023) * 1e-6
    return acc


def horner_spec(*, steps: int, x: float = 1.1) -> float:
    """``horner_core.c`` loop: ``acc = acc * x + 1``."""
    acc = 0.0
    for _ in range(steps):
        acc = acc * x + 1.0
    return acc


# --- tier-1 reference cases ---------------------------------------------------


@dataclass(frozen=True)
class Tier1Reference:
    """Expected results and harness-side anti-gaming guards we own."""

    full_n: int
    small_n: int
    compute_full: Callable[[], float]
    compute_small: Callable[[], float]
    min_abs_full: float
    min_li_seconds: float
    rtol: float = 1e-8
    atol: float = 0.0


def _ref(
    name: str,
    *,
    full_n: int,
    small_n: int,
    full_fn: Callable[[int], float],
    min_abs_full: float,
    min_li_seconds: float,
    rtol: float = 1e-8,
) -> Tier1Reference:
    return Tier1Reference(
        full_n=full_n,
        small_n=small_n,
        compute_full=lambda fn=full_fn, n=full_n: fn(n),
        compute_small=lambda fn=full_fn, n=small_n: fn(n),
        min_abs_full=min_abs_full,
        min_li_seconds=min_li_seconds,
        rtol=rtol,
    )


TIER1_REFERENCE: dict[str, Tier1Reference] = {
    "simd_dot": _ref(
        "simd_dot",
        full_n=10_000_000,
        small_n=8,
        full_fn=dot_spec_sum,
        min_abs_full=300_000.0,
        min_li_seconds=0.01,
        rtol=2e-8,
    ),
    "matmul_naive": _ref(
        "matmul_naive",
        full_n=256,
        small_n=4,
        full_fn=matmul_naive_spec_sum,
        min_abs_full=100_000.0,
        min_li_seconds=0.002,
    ),
    "matmul_blocked": _ref(
        "matmul_blocked",
        full_n=512,
        small_n=8,
        full_fn=matmul_blocked_spec_sum,
        min_abs_full=1_000_000.0,
        min_li_seconds=0.008,
    ),
    "reduce_sum": _ref(
        "reduce_sum",
        full_n=100_000_000,
        small_n=16,
        full_fn=reduce_sum_spec,
        min_abs_full=50_000.0,
        min_li_seconds=0.01,
        rtol=2e-8,
    ),
    "horner_pure_li": Tier1Reference(
        full_n=5_000_000,
        small_n=8,
        compute_full=lambda: horner_spec(steps=5_000_000),
        compute_small=lambda: horner_spec(steps=8),
        min_abs_full=1e300,
        min_li_seconds=0.001,
        rtol=0.0,
        atol=0.0,
    ),
}


def assert_checksum_against_spec(
    bench: str,
    actual_text: str,
    *,
    label: str,
    size: str,
    ref: Tier1Reference,
    use_small: bool,
) -> None:
    actual = parse_result(actual_text)
    expected = ref.compute_small() if use_small else ref.compute_full()
    min_abs = 1e-12 if use_small else ref.min_abs_full

    if math.isnan(actual):
        raise RuntimeError(f"{bench}: {label} ({size}) is NaN")
    if math.isinf(expected):
        if actual != expected:
            raise RuntimeError(
                f"{bench}: {label} ({size}) {actual_text!r} != spec {format_result(expected)!r}"
            )
        return
    if math.isinf(actual):
        raise RuntimeError(f"{bench}: {label} ({size}) unexpected infinity")
    if abs(actual) < min_abs:
        raise RuntimeError(
            f"{bench}: {label} ({size}) |result|={abs(actual)!r} below floor {min_abs!r} "
            "(likely DCE / empty kernel)"
        )

    if use_small:
        if format_result(actual) != format_result(expected):
            raise RuntimeError(
                f"{bench}: {label} ({size}) {actual_text!r} != spec {format_result(expected)!r}"
            )
        return

    if not float_close(actual, expected, rtol=ref.rtol, atol=ref.atol):
        raise RuntimeError(
            f"{bench}: {label} (full) {actual_text!r} != spec {format_result(expected)!r} "
            f"(rtol={ref.rtol})"
        )


def assert_spec_small_matches_table(bench: str, ref: Tier1Reference) -> None:
    """Sanity: embedded small goldens stay aligned with ``compute_small``."""
    assert_checksum_against_spec(
        bench,
        format_result(ref.compute_small()),
        label="spec-self",
        size="small",
        ref=ref,
        use_small=True,
    )


def tier1_has_reference(bench: str) -> bool:
    return bench in TIER1_REFERENCE
