#!/usr/bin/env python3
"""Verify studio-design-tokens.toml hex colors match packages/li-ui/src/lib.li studio tokens."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOKENS = ROOT / "docs/design/studio-design-tokens.toml"
PALETTE = ROOT / "packages/li-ui/src/lib.li"

# TOML [color] key -> lib.li studio token function name
COLOR_MAP = {
    "bg_primary": "studio_color_bg_primary",
    "bg_elevated": "studio_color_bg_elevated",
    "accent_cyan": "studio_color_accent_cyan",
    "accent_violet": "studio_color_accent_violet",
    "accent_mint": "studio_color_accent_mint",
    "agent_running": "studio_color_agent_running",
    "agent_error": "studio_color_agent_error",
}

FLOAT_MAP = {
    "dock_width_px": ("studio_dock_width_px", 56.0),
    "topbar_height_px": ("studio_topbar_height_px", 44.0),
    "inspector_width_px": ("studio_inspector_width_px", 320.0),
    "panel_transition_ms": ("studio_panel_transition_ms", 100.0),
}


def hex_to_rgb(hex_str: str) -> tuple[float, float, float]:
    h = hex_str.lstrip("#")
    if len(h) != 6:
        raise ValueError(hex_str)
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    return r / 255.0, g / 255.0, b / 255.0


def strip_toml_comment(line: str) -> str:
    in_quote = False
    quote = ""
    for i, ch in enumerate(line):
        if ch in ('"', "'") and (i == 0 or line[i - 1] != "\\"):
            if not in_quote:
                in_quote = True
                quote = ch
            elif ch == quote:
                in_quote = False
        elif ch == "#" and not in_quote:
            return line[:i].strip()
    return line.strip()


def parse_toml_section(path: Path, want: str) -> dict[str, str]:
    section = ""
    out: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = strip_toml_comment(raw)
        if not line:
            continue
        m = re.match(r"\[([^\]]+)\]", line)
        if m:
            section = m.group(1)
            continue
        if "=" not in line or section != want:
            continue
        key, _, val = line.partition("=")
        out[key.strip()] = val.strip().strip('"').strip("'")
    return out


def parse_palette_rgb(path: Path, fn: str) -> tuple[float, float, float]:
    block = path.read_text(encoding="utf-8")
    pat = rf"def {re.escape(fn)}\(\)[\s\S]*?return color_rgb\(([^)]+)\)"
    m = re.search(pat, block)
    if not m:
        raise KeyError(fn)
    parts = [float(x.strip()) for x in m.group(1).split(",")[:3]]
    return parts[0], parts[1], parts[2]


def parse_palette_float(path: Path, fn: str) -> float:
    block = path.read_text(encoding="utf-8")
    pat = rf"def {re.escape(fn)}\(\)[\s\S]*?return ([0-9.]+)"
    m = re.search(pat, block)
    if not m:
        raise KeyError(fn)
    return float(m.group(1))


def main() -> int:
    if not TOKENS.is_file():
        print(f"error: missing {TOKENS}", file=sys.stderr)
        return 1
    if not PALETTE.is_file():
        print(f"error: missing {PALETTE}", file=sys.stderr)
        return 1

    colors = parse_toml_section(TOKENS, "color")
    tol = 1.0 / 255.0
    errors: list[str] = []

    for key, fn in COLOR_MAP.items():
        hex_val = colors.get(key)
        if not hex_val:
            errors.append(f"missing TOML color.{key}")
            continue
        expected = hex_to_rgb(hex_val)
        actual = parse_palette_rgb(PALETTE, fn)
        for i, label in enumerate("rgb"):
            if abs(expected[i] - actual[i]) > tol:
                errors.append(
                    f"{fn} {label}: palette {actual[i]:.4f} != TOML {hex_val} ({expected[i]:.4f})"
                )

    spacing_raw = parse_toml_section(TOKENS, "spacing")
    spacing_section = {k: float(v) for k, v in spacing_raw.items()}
    motion_raw = parse_toml_section(TOKENS, "motion")
    motion_section = {k: float(v) for k, v in motion_raw.items()}

    for key, (fn, _) in FLOAT_MAP.items():
        src = spacing_section if key.endswith("_px") else motion_section
        tom = src.get(key)
        if tom is None:
            errors.append(f"missing TOML value for {key}")
            continue
        actual = parse_palette_float(PALETTE, fn)
        if actual != tom:
            errors.append(f"{fn}: palette {actual} != TOML {tom}")

    if errors:
        for e in errors:
            print(f"studio-ui-ux-verify-tokens: {e}", file=sys.stderr)
        return 1

    print("studio-ui-ux-verify-tokens: ok (TOML ↔ studio_palette.li)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
