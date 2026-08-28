#!/usr/bin/env python3
"""Generic physics animation entry point for benchmarks/harness.

Particle trajectories (multi-language GIF grids):
  python3 benchmarks/harness/animate_md.py --all
  python3 benchmarks/harness/animate_physics.py --benchmark md_lennard_jones

Grid / field animations (add a ``--dump`` mode on the C kernel, then extend here):
  1. In ``common/*_core.c``: optional ``--dump <path>`` writing u(x,y) or |u| per stride.
  2. Implement ``render_<bench>_grid()`` below (matplotlib imshow + plot_theme).
  3. Register the benchmark in ``GRID_BENCHES``.

Until dump exists, grid benches print setup instructions only.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HARNESS = Path(__file__).resolve().parent

PARTICLE_BENCHES: dict[str, list[str]] = {
    "md_lennard_jones": ["animate_md.py", "--all"],
}

GRID_BENCHES: tuple[str, ...] = (
    "heat_equation_2d",
    "wave_equation_1d",
    "wave_equation_2d",
    "advection_diffusion_2d",
)


def run_md_delegate(extra: list[str]) -> int:
    script = HARNESS / "animate_md.py"
    return subprocess.call([sys.executable, str(script), *extra], cwd=REPO)


def grid_instructions(name: str) -> None:
    root = REPO / "benchmarks" / "tier2_physics" / name
    print(f"{name}: grid animation not wired yet.")
    print(f"  kernel: {root / 'common'}")
    print("  steps: add --dump <file> to cpp/main.c + stride in core; then imshow frames here.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Route physics benchmark animations")
    parser.add_argument(
        "--benchmark",
        default="md_lennard_jones",
        help="tier2_physics directory name",
    )
    parser.add_argument(
        "extra",
        nargs="*",
        help="extra args forwarded to animate_md for particle benches",
    )
    args = parser.parse_args()
    name = args.benchmark

    if name in PARTICLE_BENCHES:
        base = PARTICLE_BENCHES[name]
        return run_md_delegate(base + list(args.extra))

    if name in GRID_BENCHES:
        grid_instructions(name)
        return 0

    print(f"unknown benchmark {name!r}; known particle: {list(PARTICLE_BENCHES)}")
    print(f"known grid (stub): {list(GRID_BENCHES)}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
