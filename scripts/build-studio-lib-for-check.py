#!/usr/bin/env python3
"""Rebuild packages/li-studio/src/lib.li for lic check (bde0ccb6 UX-08 + fix-check)."""
from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "packages/li-studio/src/lib.li"
BASE = "bde0ccb6"


def main() -> None:
    subprocess.run(
        ["git", "show", f"{BASE}:packages/li-studio/src/lib.li"],
        cwd=ROOT,
        check=True,
        stdout=LIB.open("w"),
    )
    subprocess.run(["python3", "scripts/fix-studio-lib-check.py"], cwd=ROOT, check=True)
    print(f"OK pipeline -> {LIB}")


if __name__ == "__main__":
    main()
