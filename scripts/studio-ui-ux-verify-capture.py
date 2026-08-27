#!/usr/bin/env python3
"""Verify studio-ui-ux-capture-progress.sh wiring and GitHub publish path (dry by default)."""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "scripts/studio-ui-ux-capture-progress.sh"
DEMO_CAPTURE = ROOT / "deploy/studio-demo/screenshots/capture.sh"
DEMO_DIR = ROOT / "deploy/studio-demo/screenshots"
STATE = ROOT / "data/studio-ui-ux-plan-loop"


def fail(msg: str) -> None:
    print(f"studio-ui-ux-verify-capture: {msg}", file=sys.stderr)
    sys.exit(1)


def check_files() -> None:
    if not CAPTURE.is_file():
        fail("missing scripts/studio-ui-ux-capture-progress.sh")
    text = CAPTURE.read_text(encoding="utf-8")
    for needle in ("capture_html", "capture_native_sdl", "publish_github", "studio-ui-ux-progress"):
        if needle not in text:
            fail(f"capture script missing expected fragment: {needle}")
    if not (ROOT / "scripts/studio-ui-ux-capture-native.sh").is_file():
        fail("missing scripts/studio-ui-ux-capture-native.sh")
    if not DEMO_CAPTURE.is_file():
        fail("missing deploy/studio-demo/screenshots/capture.sh")
    os.chmod(DEMO_CAPTURE, 0o755)
    os.chmod(CAPTURE, 0o755)
    html = sorted(DEMO_DIR.glob("[0-9]*.html"))
    if len(html) < 2:
        fail("need at least two numbered HTML mocks in deploy/studio-demo/screenshots/")
    if not (DEMO_DIR / "studio-tokens.css").is_file():
        fail("missing studio-tokens.css")
    print(f"studio-ui-ux-verify-capture: {len(html)} HTML mock(s)")


def parse_gh_defaults() -> tuple[str, str]:
    text = CAPTURE.read_text(encoding="utf-8")
    repo_m = re.search(r'REPO="\$\{STUDIO_UI_UX_GH_REPO:-([^}]+)\}"', text)
    tag_m = re.search(r'RELEASE_TAG="\$\{STUDIO_UI_UX_RELEASE_TAG:-([^}]+)\}"', text)
    repo = repo_m.group(1) if repo_m else "li-langverse/lic"
    tag = tag_m.group(1) if tag_m else "studio-ui-ux-progress"
    return repo, tag


def dry_capture() -> Path:
    env = {
        **os.environ,
        "STUDIO_UI_UX_CAPTURE_DRY": "1",
        "STUDIO_UI_UX_ITERATION": "verify-capture-dry",
        "STUDIO_UI_UX_CAPTURE_SKIP_HTML": os.environ.get(
            "STUDIO_UI_UX_VERIFY_SKIP_HTML", "0"
        ),
    }
    proc = subprocess.run(
        ["bash", str(CAPTURE)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if proc.returncode != 0:
        fail(f"dry capture exit {proc.returncode}\n{proc.stderr[-2000:]}")
    art = STATE / "artifacts/iter-verify-capture-dry"
    if not art.is_dir():
        fail(f"expected artifact dir {art}")
    report = art / "report.md"
    if not report.is_file():
        fail("dry capture did not write report.md")
    print(f"studio-ui-ux-verify-capture: dry run ok → {art}")
    return art


def check_gh_path(repo: str, tag: str) -> None:
    if not shutil_which("gh"):
        print("studio-ui-ux-verify-capture: gh missing — skip release path check")
        return
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        print("studio-ui-ux-verify-capture: no GH_TOKEN — skip live gh path check")
        return
    view = subprocess.run(
        ["gh", "release", "view", tag, "--repo", repo, "--json", "tagName,url"],
        capture_output=True,
        text=True,
    )
    if view.returncode == 0:
        data = json.loads(view.stdout or "{}")
        print(f"studio-ui-ux-verify-capture: release ok {data.get('url', tag)}")
        return
    # Release may not exist yet; verify API accepts create path
    api = subprocess.run(
        ["gh", "api", f"repos/{repo}", "-q", ".full_name"],
        capture_output=True,
        text=True,
    )
    if api.returncode != 0:
        fail(f"cannot access repo {repo}: {api.stderr.strip()}")
    print(f"studio-ui-ux-verify-capture: repo {api.stdout.strip()} reachable; release {tag} optional")


def shutil_which(cmd: str) -> str | None:
    from shutil import which

    return which(cmd)


def main() -> int:
    check_files()
    repo, tag = parse_gh_defaults()
    print(f"studio-ui-ux-verify-capture: gh repo={repo} release_tag={tag}")
    dry_capture()
    check_gh_path(repo, tag)
    print("studio-ui-ux-verify-capture: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
