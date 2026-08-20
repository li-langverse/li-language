#!/usr/bin/env python3
"""One-shot: rewrite Li sources from proc to def (extern proc becomes extern def)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP = {"build", ".git", "node_modules"}
# Negative syntax tests intentionally keep `proc` / `extern proc`; the compiler suite
# asserts they are rejected, so the migrator must never touch them.
SKIP_DIRS = {"li-tests"}


def migrate_line(line: str) -> str:
    line = re.sub(r"\basync proc\b", "async def", line)
    line = re.sub(r"\bextern proc\b", "extern def", line)
    if re.match(r"^(\s*)proc\b", line):
        return re.sub(r"^(\s*)proc\b", r"\1def", line, count=1)
    return line


def migrate_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    lines = [migrate_line(ln) for ln in text.splitlines(keepends=True)]
    new = "".join(lines)
    if new != text:
        path.write_text(new, encoding="utf-8")
        return True
    return False


def main() -> int:
    changed = 0
    for path in ROOT.rglob("*.li"):
        if any(part in SKIP for part in path.parts):
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        if migrate_file(path):
            changed += 1
    print(f"migrated {changed} .li files under {ROOT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
