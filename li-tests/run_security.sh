#!/usr/bin/env bash
# Malformed-input harness: lic must reject cleanly (exit 0/1), never crash (signals).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
# shellcheck source=../scripts/lib/li-ui.sh
source "$REPO/scripts/lib/li-ui.sh"
if [[ -z "${LIC:-}" ]]; then
  LIC="$("$REPO/scripts/resolve-lic.sh")"
fi
TIMEOUT_SEC="${LI_SECURITY_TIMEOUT:-10}"

if [[ ! -x "$LIC" ]]; then
  echo "run_security: skip (lic not executable at $LIC)"
  exit 0
fi

pass=0
fail=0

run_no_crash() {
  local label="$1"
  shift
  local out rc
  if command -v timeout >/dev/null 2>&1; then
    out="$(timeout "$TIMEOUT_SEC" "$@" 2>&1)" && rc=0 || rc=$?
  elif command -v gtimeout >/dev/null 2>&1; then
    out="$(gtimeout "$TIMEOUT_SEC" "$@" 2>&1)" && rc=0 || rc=$?
  else
    out="$("$@" 2>&1)" && rc=0 || rc=$?
  fi
  if (( rc > 128 )); then
    li_test_fail "$label (signal exit $rc)"
    echo "$out" | head -20
    fail=$((fail + 1))
    return
  fi
  if (( rc == 124 )); then
    li_test_fail "$label (timeout ${TIMEOUT_SEC}s)"
    fail=$((fail + 1))
    return
  fi
  li_test_pass "$label (exit $rc)"
  pass=$((pass + 1))
}

for f in "$ROOT"/security/*.li; do
  [[ -f "$f" ]] || continue
  base="$(basename "$f")"
  run_no_crash "parse $base" "$LIC" parse "$f"
  run_no_crash "check $base" "$LIC" check "$f"
done

# Generated stress (not committed): huge comment line
tmp="$(mktemp -t li-huge.XXXXXX.li)"
trap 'rm -f "$tmp"' EXIT
python3 -c 'print("# " + "x" * 500_000)' >"$tmp"
run_no_crash "parse huge_comment" "$LIC" parse "$tmp"
run_no_crash "check huge_comment" "$LIC" check "$tmp"

echo "--- run_security: pass=$pass fail=$fail"
li_tests_footer "$pass" "$fail" 0
[[ "$fail" -eq 0 ]]
