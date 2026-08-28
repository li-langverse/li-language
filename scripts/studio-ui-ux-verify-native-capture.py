#!/usr/bin/env python3
"""Verify native SDL/Xvfb capture wiring (soft-skip when libs missing)."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE_SH = ROOT / "scripts/studio-ui-ux-capture-native.sh"
CAPTURE_C = ROOT / "deploy/studio-demo/native/studio_viewport_capture.c"
PPM_PY = ROOT / "scripts/studio-ppm-to-png.py"
STATE = ROOT / "data/studio-ui-ux-plan-loop"


def fail(msg: str) -> None:
    print(f"studio-ui-ux-verify-native-capture: {msg}", file=sys.stderr)
    sys.exit(1)


def check_wiring() -> None:
    for p in (NATIVE_SH, CAPTURE_C, PPM_PY):
        if not p.is_file():
            fail(f"missing {p.relative_to(ROOT)}")
    text = NATIVE_SH.read_text(encoding="utf-8")
    if "STUDIO_UI_UX_NATIVE_PNG_DIR" not in text:
        fail("native capture script missing STUDIO_UI_UX_NATIVE_PNG_DIR")
    print("studio-ui-ux-verify-native-capture: wiring ok")


def try_capture() -> None:
    if os.environ.get("STUDIO_UI_UX_VERIFY_SKIP_NATIVE_RUN") == "1":
        print("studio-ui-ux-verify-native-capture: skip run (STUDIO_UI_UX_VERIFY_SKIP_NATIVE_RUN=1)")
        return
    if not shutil.which("pkg-config"):
        print("studio-ui-ux-verify-native-capture: no pkg-config — skip live capture")
        return
    proc = subprocess.run(
        ["pkg-config", "--exists", "sdl2"],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        print("studio-ui-ux-verify-native-capture: libsdl2 missing — skip live capture")
        return
    if not os.environ.get("DISPLAY") and not (
        shutil.which("xvfb-run") or shutil.which("Xvfb")
    ):
        print("studio-ui-ux-verify-native-capture: no DISPLAY/Xvfb — skip live capture")
        return
    out = STATE / "artifacts/verify-native-capture"
    png = out / "png"
    png.mkdir(parents=True, exist_ok=True)
    env = {
        **os.environ,
        "STUDIO_UI_UX_NATIVE_PNG_DIR": str(png),
        "STUDIO_VIEWPORT_CAPTURE_FRAMES": "1",
    }
    run = subprocess.run(
        ["bash", str(NATIVE_SH)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
        timeout=90,
        check=False,
    )
    meta = STATE / "latest-native-capture.json"
    if run.returncode != 0:
        print(run.stderr[-800:] if run.stderr else run.stdout)
        fail(f"native capture exit {run.returncode}")
    if not list(png.glob("*.png")):
        fail("native capture produced no PNG")
    data = json.loads(meta.read_text(encoding="utf-8"))
    if not data.get("native_pixels"):
        fail("latest-native-capture.json native_pixels false")
    print(f"studio-ui-ux-verify-native-capture: live ok ({len(list(png.glob('*.png')))} PNG)")


def main() -> int:
    check_wiring()
    try_capture()
    print("studio-ui-ux-verify-native-capture: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
