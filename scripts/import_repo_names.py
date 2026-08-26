#!/usr/bin/env python3
"""Map ergonomic import paths to canonical GitHub repo names (li-langverse org).

Convention: import `physics.relativity` → repo `li-physics-relativity`
(dots → hyphens, `li-` prefix). Monorepo folder should match the repo name.

See docs/language/import-style.md and docs/ecosystem/repo-naming.md
"""
from __future__ import annotations

import re
from pathlib import Path

# Monorepo folder (packages/*) → ergonomic import_name
LEGACY_FOLDER_IMPORT: dict[str, str] = {
    "li-std-math": "math",
    "li-std-numerics": "math.numerics",
    "li-std-core": "core",
    "li-std-ui": "ui",
    "li-std-scene": "scene",
    "li-std-physics-core": "physics.core",
    "li-std-physics-rigid": "physics.rigid",
    "li-std-physics-runtime": "physics.runtime",
    "li-std-physics-particles": "physics.particles",
    "li-std-physics-fluids": "physics.fluids",
    "li-std-physics-weather": "physics.weather",
    "li-std-physics-aero": "physics.aero",
    "li-std-physics-chem": "physics.chem",
    "li-std-physics-em": "physics.em",
    "li-std-physics-quantum": "physics.quantum",
    "li-std-physics-relativity": "physics.relativity",
    "li-std-physics-hep": "physics.hep",
    "li-httpd": "net.httpd",
    "li-net": "net",
    "li-demo": "demo",
    "li-bytes": "bytes",
    "li-rng": "rng",
    "li-prob": "prob",
    "li-crypto": "crypto",
    "li-tls": "tls",
    "li-acme": "acme",
    "li-schema": "schema",
}


def import_name_to_github_repo(import_name: str) -> str:
    """`physics.relativity` → `li-physics-relativity`."""
    slug = import_name.strip().replace(".", "-")
    if not slug:
        raise ValueError("empty import_name")
    return f"li-{slug}"


def github_repo_to_import_name(repo: str) -> str | None:
    """`li-physics-relativity` → `physics.relativity` (inverse when prefixed)."""
    if not repo.startswith("li-"):
        return None
    return repo[3:].replace("-", ".")


def legacy_folder_to_github_repo(folder: str) -> str:
    if folder in LEGACY_FOLDER_IMPORT:
        return import_name_to_github_repo(LEGACY_FOLDER_IMPORT[folder])
    if folder.startswith("li-"):
        return folder
    return import_name_to_github_repo(folder.replace("_", "."))


def canonical_folder_name(folder: str) -> str:
    """Target monorepo directory name (same as GitHub repo)."""
    return legacy_folder_to_github_repo(folder)


def read_toml_field(text: str, key: str) -> str | None:
    m = re.search(rf'^{re.escape(key)}\s*=\s*"([^"]*)"', text, re.MULTILINE)
    return m.group(1) if m else None


def read_github_repo_from_toml(path: Path) -> str | None:
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8")
    return read_toml_field(text, "github_repo") or read_toml_field(text, "import_name") and (
        import_name_to_github_repo(read_toml_field(text, "import_name") or "")
    )


def patch_package_toml(path: Path, import_name: str) -> bool:
    text = path.read_text(encoding="utf-8")
    github_repo = import_name_to_github_repo(import_name)
    orig = text

    def upsert(key: str, value: str) -> None:
        nonlocal text
        line = f'{key} = "{value}"'
        if re.search(rf"^{re.escape(key)}\s*=", text, re.MULTILINE):
            text = re.sub(rf'^{re.escape(key)}\s*=\s*"[^"]*"', line, text, flags=re.MULTILINE)
        elif "[package.metadata.li]" in text:
            text = text.replace(
                "[package.metadata.li]\n",
                f"[package.metadata.li]\n{line}\n",
            )
        else:
            text += f"\n[package.metadata.li]\n{line}\n"

    upsert("import_name", import_name)
    upsert("github_repo", github_repo)

    text = re.sub(
        r'^url\s*=\s*"https://github\.com/li-langverse/[^"]*"',
        f'url = "https://github.com/li-langverse/{github_repo}"',
        text,
        flags=re.MULTILINE,
    )
    text = re.sub(
        r"https://li-langverse\.github\.io/li-language/ecosystem/[^/\"]+/",
        f"https://li-langverse.github.io/li-language/ecosystem/{github_repo}/",
        text,
    )
    text = re.sub(
        r"https://github\.com/li-langverse/[^/]+/blob/main/",
        f"https://github.com/li-langverse/{github_repo}/blob/main/",
        text,
    )

    if text != orig:
        path.write_text(text, encoding="utf-8")
        return True
    return False


if __name__ == "__main__":
    for folder, imp in sorted(LEGACY_FOLDER_IMPORT.items()):
        repo = import_name_to_github_repo(imp)
        print(f"{folder:32} import={imp:22} github={repo}")
