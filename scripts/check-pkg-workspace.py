#!/usr/bin/env python3
"""Gate: httpd package workspace alignment (li-new-package scaffold + lip § A3).

Validates packages/li.toml lists every Phase-H infra member and each member tree
matches scripts/templates/package/ layout.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / "packages"
WORKSPACE_TOML = PACKAGES / "li.toml"

# Plan names → monorepo folder (li-httpd publishes as li-net-httpd in lic).
HTTPD_MEMBERS: list[tuple[str, str]] = [
    ("li-bytes", "li-bytes"),
    ("li-net", "li-net"),
    ("li-rng", "li-rng"),
    ("li-prob", "li-prob"),
    ("li-crypto", "li-crypto"),
    ("li-tls", "li-tls"),
    ("li-acme", "li-acme"),
    ("li-schema", "li-schema"),
    ("li-log", "li-log"),
    ("li-http", "li-http"),
    ("li-httpd", "li-net-httpd"),
]

SCAFFOLD_FILES = (
    "li.toml",
    "README.md",
    "PUBLISH.md",
    "CHANGELOG.md",
    "SECURITY.md",
    ".gitignore",
    "docs/traceability.md",
    "li-toolchain.toml",
    "li-tests/manifest.toml",
    "li-tests/smoke/builds.li",
    "src/lib.li",
)

REQUIRED_LI_TOML_KEYS = ("name", "version", "edition", "license", "description")
REQUIRED_METADATA_LI = ("min_lic", "maintainer", "pkg_id", "github_repo", "import_name")


def read_workspace_members() -> list[str]:
    text = WORKSPACE_TOML.read_text(encoding="utf-8")
    m = re.search(r"members\s*=\s*\[(.*?)\]", text, re.S)
    if not m:
        return []
    return re.findall(r'"([^"]+)"', m.group(1))


def read_toml_field(text: str, key: str, section: str | None = None) -> str | None:
    if section:
        sec_m = re.search(rf"\[{re.escape(section)}\](.*?)(?=\n\[|\Z)", text, re.S)
        if not sec_m:
            return None
        text = sec_m.group(1)
    m = re.search(rf"^{re.escape(key)}\s*=\s*\"([^\"]*)\"", text, re.MULTILINE)
    return m.group(1) if m else None


def check_package(folder: str, plan_name: str) -> list[str]:
    errors: list[str] = []
    pkg_dir = PACKAGES / folder
    if not pkg_dir.is_dir():
        return [f"missing directory packages/{folder} (plan member {plan_name})"]

    for rel in SCAFFOLD_FILES:
        if not (pkg_dir / rel).is_file():
            errors.append(f"packages/{folder}: missing scaffold file {rel}")

    toml_path = pkg_dir / "li.toml"
    text = toml_path.read_text(encoding="utf-8")
    for key in REQUIRED_LI_TOML_KEYS:
        if read_toml_field(text, key) is None:
            errors.append(f"packages/{folder}/li.toml: missing [package].{key}")

    edition = read_toml_field(text, "edition")
    if edition != "2026":
        errors.append(f"packages/{folder}/li.toml: edition must be 2026, got {edition!r}")

    if "[package.metadata.li]" not in text:
        errors.append(f"packages/{folder}/li.toml: missing [package.metadata.li]")
    else:
        meta = text.split("[package.metadata.li]", 1)[-1].split("[", 1)[0]
        for key in REQUIRED_METADATA_LI:
            if not re.search(rf"^{re.escape(key)}\s*=", meta, re.MULTILINE):
                errors.append(f"packages/{folder}/li.toml: missing metadata.li {key}")

    pkg_name = read_toml_field(text, "name")
    if pkg_name != folder and folder != "li-net-httpd":
        errors.append(f"packages/{folder}/li.toml: name={pkg_name!r} != folder")
    if folder == "li-net-httpd" and pkg_name != "li-net-httpd":
        errors.append(f"packages/li-net-httpd/li.toml: name must be li-net-httpd")

    if "[package.repository]" not in text:
        errors.append(f"packages/{folder}/li.toml: missing [package.repository]")

    return errors


def main() -> int:
    errors: list[str] = []
    members = read_workspace_members()

    for plan_name, folder in HTTPD_MEMBERS:
        if folder not in members:
            errors.append(f"packages/li.toml: workspace missing member {folder!r} ({plan_name})")
        errors.extend(check_package(folder, plan_name))

    if errors:
        print("check-pkg-workspace: FAIL", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print(f"check-pkg-workspace: OK ({len(HTTPD_MEMBERS)} httpd members)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
