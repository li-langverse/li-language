#!/usr/bin/env python3
"""Append a package name to [workspace].members in li.toml."""
import pathlib
import re
import sys


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: li-workspace-add-member.py WORKSPACE_LI_TOML NAME", file=sys.stderr)
        sys.exit(1)
    path = pathlib.Path(sys.argv[1])
    name = sys.argv[2]
    if f'"{name}"' in path.read_text():
        return
    text = path.read_text() if path.exists() else "[workspace]\nmembers = []\nresolver = \"path\"\n"
    if re.search(r"members\s*=\s*\[", text):
        text = re.sub(
            r"(members\s*=\s*\[)([^\]]*)",
            rf'\1"{name}", \2',
            text,
            count=1,
        )
    else:
        text += f'\nmembers = ["{name}"]\n'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


if __name__ == "__main__":
    main()
