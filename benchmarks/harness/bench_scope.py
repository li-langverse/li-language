#!/usr/bin/env python3
"""Resolve which benchmarks / tests to run for a package or git diff (modular scope)."""

from __future__ import annotations

import argparse
import fnmatch
import subprocess
import sys
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / "benchmarks" / "manifest.toml"


def _load_manifest() -> dict[str, Any]:
    try:
        import tomllib
    except ImportError:
        import tomli as tomllib  # type: ignore

    return tomllib.loads(MANIFEST.read_text(encoding="utf-8"))


def packages_table(doc: dict[str, Any]) -> dict[str, dict[str, Any]]:
    out: dict[str, dict[str, Any]] = {}
    for row in doc.get("package", []):
        out[row["name"]] = row
    return out


def expand_packages(names: list[str], table: dict[str, dict[str, Any]]) -> set[str]:
    expanded: set[str] = set(names)
    changed = True
    while changed:
        changed = False
        for name in list(expanded):
            row = table.get(name)
            if not row:
                continue
            for dep in row.get("depends_on", []):
                if dep not in expanded:
                    expanded.add(dep)
                    changed = True
    return expanded


def benches_for_packages(
    pkg_names: set[str], table: dict[str, dict[str, Any]]
) -> tuple[set[str], set[str], set[str]]:
    benches: set[str] = set()
    hooks: set[str] = set()
    composable: set[str] = set()
    for name in pkg_names:
        row = table.get(name)
        if not row:
            continue
        benches.update(row.get("benches", []))
        hooks.update(row.get("hooks", []))
        composable.update(row.get("composable", []))
    return benches, hooks, composable


def packages_from_changed_paths(paths: list[str], doc: dict[str, Any]) -> set[str]:
    table = packages_table(doc)
    hit: set[str] = set()
    for path in paths:
        p = path.replace("\\", "/")
        for row in doc.get("package", []):
            for prefix in row.get("paths", []):
                if p.startswith(prefix.rstrip("/")) or fnmatch.fnmatch(p, prefix):
                    hit.add(row["name"])
        for rule in doc.get("path_rule", []):
            glob = rule.get("glob", "")
            if glob and fnmatch.fnmatch(p, glob):
                hit.update(rule.get("packages", []))
    if not hit:
        return set()
    return expand_packages(sorted(hit), table)


def git_changed_files(base: str = "WORKTREE") -> list[str]:
    files: set[str] = set()
    if base == "WORKTREE":
        for cmd in (
            ["git", "diff", "--name-only"],
            ["git", "diff", "--name-only", "--cached"],
            ["git", "ls-files", "--others", "--exclude-standard"],
        ):
            try:
                out = subprocess.check_output(cmd, cwd=REPO, text=True)
            except subprocess.CalledProcessError:
                continue
            files.update(ln.strip() for ln in out.splitlines() if ln.strip())
        return sorted(files)
    ref = base if base != "HEAD" else "HEAD~1"
    try:
        out = subprocess.check_output(
            ["git", "diff", "--name-only", ref], cwd=REPO, text=True
        )
    except subprocess.CalledProcessError:
        out = ""
    return [ln.strip() for ln in out.splitlines() if ln.strip()]


def resolve_scope(
    *,
    packages: list[str] | None = None,
    benches: list[str] | None = None,
    changed: bool = False,
    changed_base: str = "WORKTREE",
) -> dict[str, Any]:
    doc = _load_manifest()
    table = packages_table(doc)
    pkg_set: set[str] = set()

    if packages:
        pkg_set = expand_packages(packages, table)
    if changed:
        paths = git_changed_files(changed_base)
        pkg_set |= packages_from_changed_paths(paths, doc)

    bench_set, hooks, composable = benches_for_packages(pkg_set, table)
    if benches:
        bench_set |= set(benches)

    return {
        "packages": sorted(pkg_set),
        "benches": sorted(bench_set),
        "hooks": sorted(hooks),
        "composable": sorted(composable),
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--package", action="append", dest="packages", default=[])
    p.add_argument("--bench", action="append", dest="benches", default=[])
    p.add_argument("--changed", action="store_true")
    p.add_argument("--changed-base", default="WORKTREE")
    p.add_argument("--print-benches", action="store_true")
    p.add_argument("--print-packages", action="store_true")
    p.add_argument("--print-composable", action="store_true")
    p.add_argument("--json", action="store_true")
    args = p.parse_args()

    scope = resolve_scope(
        packages=args.packages or None,
        benches=args.benches or None,
        changed=args.changed,
        changed_base=args.changed_base,
    )

    if args.json:
        import json

        print(json.dumps(scope, indent=2))
        return 0
    if args.print_packages:
        print("\n".join(scope["packages"]))
        return 0
    if args.print_composable:
        print("\n".join(scope["composable"]))
        return 0
    if args.print_benches:
        print(",".join(scope["benches"]))
        return 0

    print(f"packages: {', '.join(scope['packages']) or '(none)'}")
    print(f"benches: {', '.join(scope['benches']) or '(none)'}")
    print(f"composable: {', '.join(scope['composable']) or '(none)'}")
    print(f"hooks: {', '.join(scope['hooks']) or '(none)'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
