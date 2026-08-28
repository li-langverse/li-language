#!/usr/bin/env python3
"""Emit deploy/studio-demo/screenshots/studio-tokens.css from studio-design-tokens.toml."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOKENS = ROOT / "docs/design/studio-design-tokens.toml"
OUT = ROOT / "deploy/studio-demo/screenshots/studio-tokens.css"


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


def parse_toml_flat(path: Path) -> dict[str, str]:
    data: dict[str, str] = {}
    section = ""
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = strip_toml_comment(raw)
        if not line:
            continue
        m = re.match(r"\[([^\]]+)\]", line)
        if m:
            section = m.group(1)
            continue
        if "=" not in line:
            continue
        key, _, val = line.partition("=")
        key = key.strip()
        val = val.strip().strip('"').strip("'")
        full = f"{section}.{key}" if section else key
        data[full] = val
    return data


def css_var_name(key: str) -> str:
    return "--" + key.replace(".", "-").replace("_", "-")


def main() -> int:
    if not TOKENS.is_file():
        print(f"error: missing {TOKENS}", file=sys.stderr)
        return 1
    t = parse_toml_flat(TOKENS)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* AUTO-GENERATED — do not edit. Run ./scripts/studio-ui-ux-generate-design-system.sh */",
        ":root {",
    ]
    for key in sorted(t):
        if key.startswith("meta.") or key.startswith("ph_ux."):
            continue
        val = t[key]
        if val.startswith("#"):
            lines.append(f"  {css_var_name(key)}: {val};")
        elif val.isdigit():
            unit = "px" if key.endswith("_px") or ".spacing." in f".{key}." else ""
            lines.append(f"  {css_var_name(key)}: {val}{unit};")
        else:
            safe = val.replace(";", "\\3b ")
            lines.append(f"  {css_var_name(key)}: {safe};")
    lines.extend(
        [
            "  --focus-ring: 0 0 0 2px var(--color-accent-cyan);",
            "  --mock-banner-bg: #3d1f00;",
            "}",
            "",
            "@media (prefers-reduced-motion: reduce) {",
            "  *, *::before, *::after { animation-duration: 0.01ms !important; transition-duration: 0.01ms !important; }",
            "}",
            "",
        ]
    )
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"studio-ui-ux-emit-demo-css: wrote {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
