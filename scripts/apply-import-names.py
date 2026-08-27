#!/usr/bin/env python3
"""Set import_name + github_repo metadata for ergonomic imports."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / "packages"

sys.path.insert(0, str(ROOT / "scripts"))
from import_repo_names import LEGACY_FOLDER_IMPORT, patch_package_toml  # noqa: E402


def main() -> int:
    changed = 0
    for folder, name in sorted(LEGACY_FOLDER_IMPORT.items()):
        toml = PACKAGES / folder / "li.toml"
        if not toml.is_file():
            continue
        if patch_package_toml(toml, name):
            print(f"  {folder} -> import={name}")
            changed += 1
    print(f"apply-import-names: updated {changed} packages")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
