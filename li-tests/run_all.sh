#!/usr/bin/env bash
# Run all li-tests suites, or the named suite filters.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
LIC="${LIC:-$REPO/build/compiler/lic/lic}"
FILTERS=()
CI="${CI:-false}"
for arg in "$@"; do
  if [[ "$arg" == "--ci" ]]; then
    CI=true
  else
    FILTERS+=("$arg")
  fi
done

should_run_suite() {
  if ((${#FILTERS[@]} == 0)); then
    return 0
  fi
  for filter in "${FILTERS[@]}"; do
    if [[ "$filter" == "all" || "$filter" == "$1" ]]; then
      return 0
    fi
  done
  return 1
}

if [[ ! -x "$LIC" ]]; then
  echo "li-tests: skip (lic not executable at $LIC)"
  exit 0
fi

pass=0
fail=0
skip=0

run_one() {
  local suite="$1" file="$2" outcome="$3" substr="${4:-}" exp_exit="${5:-}"

  local path="$ROOT/$file"
  if [[ ! -f "$path" ]]; then
    echo "SKIP missing $file"
    skip=$((skip + 1))
    return
  fi

  local exp_file="${path%.li}.exp"
  if [[ -f "$exp_file" ]]; then
    substr="$(head -1 "$exp_file")"
  fi

  case "$outcome" in
    parse_ok)
      if "$LIC" parse "$path" >/dev/null 2>&1; then
        echo "PASS parse_ok $file"
        pass=$((pass + 1))
      else
        echo "FAIL parse_ok $file"
        fail=$((fail + 1))
      fi
      ;;
    parse_fail)
      if "$LIC" parse "$path" >/dev/null 2>&1; then
        echo "FAIL parse_fail $file (should reject)"
        fail=$((fail + 1))
      else
        echo "PASS parse_fail $file"
        pass=$((pass + 1))
      fi
      ;;
    compile_ok|verify_ok)
      if "$LIC" build "$path" -o /dev/null 2>/dev/null; then
        echo "PASS $outcome $file"
        pass=$((pass + 1))
      else
        echo "FAIL $outcome $file"
        fail=$((fail + 1))
      fi
      ;;
    compile_fail|verify_fail)
      local err
      err="$("$LIC" build "$path" -o /dev/null 2>&1)" || true
      if "$LIC" build "$path" -o /dev/null 2>/dev/null; then
        echo "FAIL $outcome $file (should reject)"
        fail=$((fail + 1))
      elif [[ -n "$substr" ]] && ! echo "$err" | grep -qi "$substr"; then
        echo "FAIL $outcome $file (missing expected substring: $substr)"
        fail=$((fail + 1))
      else
        echo "PASS $outcome $file"
        pass=$((pass + 1))
      fi
      ;;
    run_exit)
      # Build, execute, and compare the process exit code to expected_exit
      # (the documented contract of the composable probes' `main`).
      local exe
      exe="$(mktemp "${TMPDIR:-/tmp}/li-run-XXXXXX")"
      if ! "$LIC" build "$path" -o "$exe" >/dev/null 2>&1; then
        echo "FAIL run_exit $file (build failed)"
        fail=$((fail + 1))
        rm -f "$exe"
        return
      fi
      local rc=0
      "$exe" >/dev/null 2>&1 || rc=$?
      rm -f "$exe"
      if [[ -n "$exp_exit" && "$rc" != "$exp_exit" ]]; then
        echo "FAIL run_exit $file (exit $rc, expected $exp_exit)"
        fail=$((fail + 1))
      else
        echo "PASS run_exit $file (exit $rc)"
        pass=$((pass + 1))
      fi
      ;;
    *)
      echo "unknown outcome $outcome for $file"
      fail=$((fail + 1))
      ;;
  esac
}

while IFS= read -r line; do
  if [[ "$line" == "[[tests]]" ]]; then
    if [[ -n "${cur_file:-}" && -n "${cur_outcome:-}" ]]; then
      if should_run_suite "$cur_suite"; then
        run_one "$cur_suite" "$cur_file" "$cur_outcome" "${cur_substr:-}" "${cur_exit:-}"
      fi
    fi
    cur_suite="" cur_file="" cur_outcome="" cur_substr="" cur_exit=""
    continue
  fi
  [[ "$line" =~ ^suite\ =\ \"(.*)\"$ ]] && cur_suite="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^file\ =\ \"(.*)\"$ ]] && cur_file="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^outcome\ =\ \"(.*)\"$ ]] && cur_outcome="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^expected_substr\ =\ \"(.*)\"$ ]] && cur_substr="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^expected_exit\ =\ \"?([0-9]+)\"?$ ]] && cur_exit="${BASH_REMATCH[1]}" && continue
done < "$ROOT/manifest.toml"

# last entry
if [[ -n "${cur_file:-}" && -n "${cur_outcome:-}" ]]; then
  if should_run_suite "$cur_suite"; then
    run_one "$cur_suite" "$cur_file" "$cur_outcome" "${cur_substr:-}" "${cur_exit:-}"
  fi
fi

echo "--- li-tests: pass=$pass fail=$fail skip=$skip"
[[ "$fail" -eq 0 ]]
