#!/usr/bin/env python3
"""Align monorepo package folders and metadata with import-based GitHub repo names.

  python3 scripts/align-package-repo-names.py --dry-run
  python3 scripts/align-package-repo-names.py --apply

Renames packages/<legacy>/ → packages/<li-physics-relativity>/ etc.,
updates packages/li.toml members and path dependencies.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / "packages"
WORKSPACE = PACKAGES / "li.toml"

sys.path.insert(0, str(ROOT / "scripts"))
from import_repo_names import (  # noqa: E402
    LEGACY_FOLDER_IMPORT,
    canonical_folder_name,
    import_name_to_github_repo,
    patch_package_toml,
)


def replace_path_deps(text: str, renames: dict[str, str]) -> str:
    for old, new in sorted(renames.items(), key=lambda x: -len(x[0])):
        text = text.replace(f'../{old}"', f'../{new}"')
        text = text.replace(f'../{old}/', f'../{new}/')
        text = text.replace(f'path = "../{old}"', f'path = "../{new}"')
    return text


def update_workspace_members(members: list[str], renames: dict[str, str]) -> list[str]:
    out = []
    for m in members:
        out.append(renames.get(m, m))
    return sorted(set(out))


def parse_members(text: str) -> list[str]:
    m = re.search(r"members\s*=\s*\[(.*?)\]", text, re.DOTALL)
    if not m:
        return []
    inner = m.group(1)
    return [x.strip().strip('"') for x in inner.split(",") if x.strip().strip('"')]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--dry-run", action="store_true", default=True)
    parser.add_argument("--metadata-only", action="store_true", help="only patch li.toml metadata")
    args = parser.parse_args()
    if args.apply:
        args.dry_run = False

    renames: dict[str, str] = {}
    for folder in LEGACY_FOLDER_IMPORT:
        target = canonical_folder_name(folder)
        if folder != target and (PACKAGES / folder).is_dir():
            renames[folder] = target

    if not renames and not args.metadata_only:
        print("align-package-repo-names: folders already aligned")
    else:
        for old, new in sorted(renames.items()):
            imp = LEGACY_FOLDER_IMPORT[old]
            print(f"  {old} -> {new}  (import {imp}, github {import_name_to_github_repo(imp)})")

    for folder, imp in LEGACY_FOLDER_IMPORT.items():
        pkg = PACKAGES / folder
        if not pkg.is_dir() and renames.get(folder):
            pkg = PACKAGES / renames[folder]
        toml = pkg / "li.toml"
        if toml.is_file():
            if args.dry_run:
                print(f"  would patch metadata: {toml.relative_to(ROOT)}")
            elif patch_package_toml(toml, imp):
                print(f"  patched metadata: {toml.relative_to(ROOT)}")

    if args.metadata_only:
        return 0

    if not renames:
        return 0

    if args.dry_run:
        print("align-package-repo-names: re-run with --apply to rename directories")
        return 0

    for old, new in renames.items():
        src = PACKAGES / old
        dst = PACKAGES / new
        if dst.exists():
            print(f"skip rename {old}: {new} exists", file=sys.stderr)
            continue
        print(f"rename {old} -> {new}")
        shutil.move(str(src), str(dst))
        toml = dst / "li.toml"
        if toml.is_file():
            text = toml.read_text(encoding="utf-8")
            text = re.sub(rf'^name\s*=\s*"{re.escape(old)}"', f'name = "{new}"', text, flags=re.M)
            text = re.sub(
                rf"PKG-{re.escape(old)}",
                f"PKG-{new}",
                text,
            )
            text = replace_path_deps(text, renames)
            toml.write_text(text, encoding="utf-8")
            patch_package_toml(toml, LEGACY_FOLDER_IMPORT[old])

    for pkg_dir in PACKAGES.iterdir():
        if not pkg_dir.is_dir():
            continue
        toml = pkg_dir / "li.toml"
        if not toml.is_file():
            continue
        text = toml.read_text(encoding="utf-8")
        new_text = replace_path_deps(text, renames)
        if new_text != text:
            toml.write_text(new_text, encoding="utf-8")
            print(f"  updated deps: {toml.relative_to(ROOT)}")

    if WORKSPACE.is_file():
        text = WORKSPACE.read_text(encoding="utf-8")
        members = parse_members(text)
        new_members = update_workspace_members(members, renames)
        new_inner = ", ".join(f'"{m}"' for m in new_members)
        new_text = re.sub(r"members\s*=\s*\[.*?\]", f"members = [{new_inner}]", text, count=1, flags=re.DOTALL)
        WORKSPACE.write_text(new_text, encoding="utf-8")
        print(f"updated {WORKSPACE.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
