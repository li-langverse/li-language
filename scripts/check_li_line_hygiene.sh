#!/usr/bin/env bash
# Line-hygiene guard: catch the merged-statement corruption class where the
# editing tool collapsed two statements onto one physical line (e.g.
# `sym_ty[tp] = 13        sym_n[0] = sym_n[0] + 1`). One such merge cost a
# hung gate on 2026-10-17 (unincremented `i` in mir_count_parallel); the
# merged form is never valid Li syntax even where the host parser tolerates
# it, so any hit is a defect.
#
# Detector: on a non-comment code line, more than one spaced assignment
# ` = ` (single equals with spaces, not ==/<=/>=/!=/=>). Measured
# zero-false-positive over the 2026-10-17 corpus. Comment-only lines are
# skipped; the guard covers .li sources only (the same-root-cause glue
# repair applied to docs/nightly-inversion-catalog.md is not machine-checked
# here).
#
# Usage:
#   scripts/check_li_line_hygiene.sh            # scan the bootstrap sources
#   scripts/check_li_line_hygiene.sh FILE...    # scan extra files too
#
# Exit codes: 0 clean, 1 hits found (each hit printed as file:line: text).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

DEFAULT_SOURCES=(
  "$ROOT/bootstrap/lic/main.li"
  "$ROOT/bootstrap/prover/main.li"
)

FILES=()
for f in "${DEFAULT_SOURCES[@]}"; do
  [[ -f "$f" ]] || { echo "check_li_line_hygiene: missing default source $f" >&2; exit 1; }
  FILES+=("$f")
done
for f in "$@"; do
  [[ -f "$f" ]] || { echo "check_li_line_hygiene: file not found: $f" >&2; exit 1; }
  FILES+=("$f")
done

python3 - "${FILES[@]}" <<'PYEOF'
import re
import sys

# Two spaced single-equals on one code line. Whitespace-adjacent excludes
# == <= >= != =>; leading [^=!<>\-] excludes the right-hand side of ==.
pat = re.compile(r"[^=!<>-] = [^=\s]")

hits = 0
for path in sys.argv[1:]:
    with open(path, encoding="utf-8", errors="replace") as fh:
        for lineno, line in enumerate(fh, 1):
            stripped = line.rstrip("\n")
            code = stripped.lstrip()
            if code.startswith("#"):
                continue
            if len(pat.findall(stripped)) >= 2:
                hits += 1
                print(f"{path}:{lineno}: {stripped.strip()[:100]}")

if hits:
    print(f"check_li_line_hygiene: {hits} merged-statement line(s) found", file=sys.stderr)
    sys.exit(1)
print("check_li_line_hygiene: ok")
PYEOF