#!/usr/bin/env bash
# Run all li-tests suites or one suite by name.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
LIC="${LIC:-$REPO/build/compiler/lic/lic}"
FILTER="${1:-all}"
CI="${CI:-false}"

if [[ "${1:-}" == "--ci" ]]; then
  CI=true
  FILTER="all"
elif [[ "${2:-}" == "--ci" ]]; then
  CI=true
fi

if [[ ! -x "$LIC" ]]; then
  echo "li-tests: skip (lic not executable at $LIC)"
  exit 0
fi

pass=0
fail=0
skip=0

run_one() {
  local suite="$1" file="$2" outcome="$3" substr="${4:-}"

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
    *)
      echo "unknown outcome $outcome for $file"
      fail=$((fail + 1))
      ;;
  esac
}

while IFS= read -r line; do
  if [[ "$line" == "[[tests]]" ]]; then
    if [[ -n "${cur_file:-}" && -n "${cur_outcome:-}" ]]; then
      if [[ "$FILTER" == "all" || "$FILTER" == "$cur_suite" ]]; then
        run_one "$cur_suite" "$cur_file" "$cur_outcome" "${cur_substr:-}"
      fi
    fi
    cur_suite="" cur_file="" cur_outcome="" cur_substr=""
    continue
  fi
  [[ "$line" =~ ^suite\ =\ \"(.*)\"$ ]] && cur_suite="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^file\ =\ \"(.*)\"$ ]] && cur_file="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^outcome\ =\ \"(.*)\"$ ]] && cur_outcome="${BASH_REMATCH[1]}" && continue
  [[ "$line" =~ ^expected_substr\ =\ \"(.*)\"$ ]] && cur_substr="${BASH_REMATCH[1]}" && continue
done < "$ROOT/manifest.toml"

# last entry
if [[ -n "${cur_file:-}" && -n "${cur_outcome:-}" ]]; then
  if [[ "$FILTER" == "all" || "$FILTER" == "$cur_suite" ]]; then
    run_one "$cur_suite" "$cur_file" "$cur_outcome" "${cur_substr:-}"
  fi
fi

echo "--- li-tests: pass=$pass fail=$fail skip=$skip"
[[ "$fail" -eq 0 ]]
