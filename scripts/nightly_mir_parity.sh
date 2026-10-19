#!/usr/bin/env bash
# Nightly MIR parity sweep: compare Li walker vs C++ lic mir on every
# parseable .li file.  Reports new regressions (files that changed from
# MATCH to DIFF since the last baseline) and opens a GitHub issue if the
# gap count increases.
#
# Usage:
#   scripts/nightly_mir_parity.sh              # run sweep, print report
#   scripts/nightly_mir_parity.sh --open-issue  # also open a GH issue
#
# Exit codes:
#   0  no regression (gap count <= baseline)
#   1  regression detected (gap count > baseline)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
LI="${LI_CHECK_BIN:-$ROOT/build/li-mir-walker}"
BASELINE_FILE="${ROOT}/.mir_parity_baseline"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$LI" ]]; then
  echo "ERROR: Li MIR walker binary not found at $LI" >&2
  echo "       Build it first: lic build bootstrap/lic/main.li -o $LI" >&2
  exit 1
fi

# Preflight: a nearly-full data volume SILENTLY scrambles rc capture inside
# census loops (small redirects under a full /tmp fail without aborting the
# loop; the sweep keeps going and rejects that the C++ host accepts show up
# as phantom walker-reject entries — recorded in docs after the 2026-10-17
# census run). Abort clearly instead of producing an untrustworthy report.
MIN_FREE_KB=$((64 * 1024))
avail_kb="$(df -k "$ROOT" | awk 'NR==2{print $4}')"
if (( avail_kb < MIN_FREE_KB )); then
  echo "ERROR: volume holding $ROOT has only ${avail_kb}KB free (< ${MIN_FREE_KB}KB)." >&2
  echo "       Free space first: with the disk this full, per-file rc capture and" >&2
  echo "       /tmp writes fail silently and the census produces phantom entries" >&2
  echo "       (see docs/nightly-inversion-catalog.md, 2026-10-17 note)." >&2
  exit 1
fi

# Line-hygiene preflight: a merged-statement line (two statements on one
# physical line) is the corruption class that hung the 2026-10-17 gate via
# an unincremented loop counter. Fail fast before sweeping.
if ! "$ROOT/scripts/check_li_line_hygiene.sh"; then
  echo "ERROR: line-hygiene guard failed; fix the reported lines first." >&2
  exit 1
fi

echo "=== Nightly MIR Parity Sweep ==="
echo "C++ : $LIC"
echo "Li  : $LI"
echo ""

checked=0
match=0
diff_count=0
skip=0
new_regressions=()
new_fixes=()

# Load previous baseline if it exists
declare -A prev_status
if [[ -f "$BASELINE_FILE" ]]; then
  while IFS=' ' read -r status filepath; do
    prev_status["$filepath"]="$status"
  done < "$BASELINE_FILE"
fi

# Current results
declare -A curr_status

# Abort helper: a nearly-full data volume produces PHANTOM verdicts (rc!=0
# from ENOSPC looks identical to a genuine reject — the 2026-10-17 census
# recorded that class, and the 2026-10-19 run aborted mid-sweep on exactly
# the same shape). Any I/O-error signature in the probe stderr, or a failed
# rc!=0 capture, is a hard abort with exit 2, not a datapoint.
abort_vol_full() {
  echo "ERROR: capture failed under a nearly-full volume — sweep aborted:" >&2
  echo "       $1" >&2
  echo "       Free space first (df -h); a phantom verdict is worse than no" >&2
  echo "       report (see docs/nightly-inversion-catalog.md, 2026-10-17 note)." >&2
  exit 2
}

while IFS= read -r f; do
  checked=$((checked + 1))
  # Mid-sweep free-space guard: the preflight above passes before the
  # churn (2026-10-19: 191MB free at start, ENOSPC mid-sweep) — other
  # consumers ebb and flow under us, so re-check every 50 files.
  if (( checked % 50 == 0 )); then
    avail_now="$(df -k "$ROOT" | awk 'NR==2{print $4}')"
    if (( avail_now < MIN_FREE_KB )); then
      abort_vol_full "free space fell to ${avail_now}KB after $checked files"
    fi
  fi
  rm -f "$TMP/probe.err"
  if "$LIC" mir "$f" >/dev/null 2>"$TMP/probe.err"; then
    cpp=1
  elif grep -qiE 'No space left|ENOSPC|I/O error' "$TMP/probe.err" 2>/dev/null; then
    abort_vol_full "C++ mir probe on $f hit an I/O error, not a verdict"
  fi
  if "$LI" mir "$f" >/dev/null 2>"$TMP/probe.err"; then
    li=1
  elif grep -qiE 'No space left|ENOSPC|I/O error' "$TMP/probe.err" 2>/dev/null; then
    abort_vol_full "Li mir probe on $f hit an I/O error, not a verdict"
  fi

  if [[ "$cpp" == "0" && "$li" == "0" ]]; then
    skip=$((skip + 1))
    curr_status["$f"]="SKIP"
    continue
  fi
  if [[ "$cpp" == "1" && "$li" == "0" ]]; then
    skip=$((skip + 1))
    curr_status["$f"]="SKIP"
    continue
  fi
  if [[ "$cpp" == "0" && "$li" == "1" ]]; then
    skip=$((skip + 1))
    curr_status["$f"]="SKIP"
    continue
  fi

  # Both sides accepted the file (rc 1 above), so the capture below must
  # succeed and exit 0; anything else is an environment failure.
  rm -f "$TMP/cpp.txt" "$TMP/li.txt"
  if ! "$LIC" mir "$f" > "$TMP/cpp.txt" 2>/dev/null; then
    abort_vol_full "C++ mir capture on $f failed (expected rc 0)"
  fi
  if ! "$LI" mir "$f" > "$TMP/li.txt" 2>/dev/null; then
    abort_vol_full "Li mir capture on $f failed (expected rc 0)"
  fi

  if diff -q "$TMP/cpp.txt" "$TMP/li.txt" >/dev/null 2>&1; then
    match=$((match + 1))
    curr_status["$f"]="MATCH"
  else
    diff_count=$((diff_count + 1))
    curr_status["$f"]="DIFF"
    cl=$(wc -l < "$TMP/cpp.txt")
    ll=$(wc -l < "$TMP/li.txt")
    echo "  DIFF  $cl/$ll  $f"
  fi
done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
           -name "*.li" -type f \
           -not -path "$ROOT/bootstrap/lic/main.li" \
           2>/dev/null | sort)

# Detect regressions and fixes
for f in "${!curr_status[@]}"; do
  curr="${curr_status[$f]}"
  prev="${prev_status[$f]:-NEW}"
  if [[ "$curr" == "DIFF" && "$prev" == "MATCH" ]]; then
    new_regressions+=("$f")
  elif [[ "$curr" == "MATCH" && "$prev" == "DIFF" ]]; then
    new_fixes+=("$f")
  fi
done

echo ""
echo "=== Summary ==="
echo "  Checked : $checked files"
echo "  MATCH   : $match (byte-exact MIR parity)"
echo "  DIFF    : $diff_count (known gaps)"
echo "  SKIP    : $skip (C++ or Li reject)"
echo ""

if [[ ${#new_regressions[@]} -gt 0 ]]; then
  echo "  NEW REGRESSIONS: ${#new_regressions[@]}"
  for r in "${new_regressions[@]}"; do
    echo "    - $r"
  done
  echo ""
fi

if [[ ${#new_fixes[@]} -gt 0 ]]; then
  echo "  NEW FIXES: ${#new_fixes[@]}"
  for r in "${new_fixes[@]}"; do
    echo "    + $r"
  done
  echo ""
fi

# Save current results as new baseline
for f in "${!curr_status[@]}"; do
  echo "${curr_status[$f]} $f"
done > "$BASELINE_FILE"

# Open GitHub issue if regression detected
if [[ "${1:-}" == "--open-issue" && ${#new_regressions[@]} -gt 0 ]]; then
  echo "Opening GitHub issue for regression..."
  body="## MIR Parity Regression Detected\n\n"
  body+="The nightly MIR parity sweep detected **${#new_regressions[@]} new regression(s)**.\n\n"
  body+="| Metric | Count |\n|--------|-------|\n"
  body+="| Total checked | $checked |\n"
  body+="| MATCH (parity) | $match |\n"
  body+="| DIFF (known gaps) | $diff_count |\n"
  body+="| New regressions | ${#new_regressions[@]} |\n"
  body+="| New fixes | ${#new_fixes[@]} |\n\n"
  body+="### Regressed files\n\n"
  for r in "${new_regressions[@]}"; do
    body+="- \`$r\`\n"
  done
  body+="\n### Fixed files\n\n"
  for r in "${new_fixes[@]}"; do
    body+="- \`$r\`\n"
  done
  body+="\n---\n*Generated by scripts/nightly_mir_parity.sh*"

  if command -v gh &>/dev/null; then
    gh issue create \
      --title "MIR Parity Regression: ${#new_regressions[@]} new gap(s)" \
      --body "$(echo -e "$body")" \
      --label "bug,mir-parity" \
      2>/dev/null || echo "  (gh issue creation failed — run manually)"
  else
    echo "  (gh CLI not found — run 'gh issue create' manually)"
  fi
fi

# Exit code
if [[ ${#new_regressions[@]} -gt 0 ]]; then
  exit 1
fi
exit 0
