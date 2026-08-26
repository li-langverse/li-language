#!/usr/bin/env bash
# Smoke: lic JSON diagnostics for agents (Vision-LLM / agent-first).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
BAD="$ROOT/li-tests/typecheck/bad_array_index.li"
GOOD="$ROOT/li-tests/typecheck/fib.li"
WARN="$ROOT/li-tests/advisory/check_deny_warn.li"
MIXED="$ROOT/li-tests/advisory/check_mixed.li"

fail() {
  echo "diagnose_json_smoke: $*" >&2
  exit 1
}

out="$("$LIC" check --format=json "$BAD" 2>&1 || true)"
echo "$out" | grep -q '"ok":false' || fail "expected ok:false — output: ${out:0:400}"
echo "$out" | grep -q '"version":1' || fail "missing version"
echo "$out" | grep -q '"schema":"diagnostic-v1"' || fail "missing schema"
echo "$out" | grep -q '"tool":"lic"' || fail "missing tool"
echo "$out" | grep -q '"command":"check"' || fail "missing command:check"
echo "$out" | grep -Eq '"code":"(type\.index|E0201)"' || fail "expected type.index — output: ${out:0:400}"
echo "$out" | grep -q 'array index out of range' || fail "missing index diagnostic message: ${out:0:400}"
echo "$out" | grep -q '"line":' || fail "missing diagnostic line"
echo "$out" | grep -q '"column":' || fail "missing diagnostic column"
echo "$out" | grep -q '"offset":' || fail "missing diagnostic offset"
echo "$out" | grep -q '"fix_hint":' || fail "missing fix_hint (null or object)"

diag="$("$LIC" diagnose "$BAD" 2>&1 || true)"
echo "$diag" | grep -q '"command":"diagnose"' || fail "diagnose envelope: ${diag:0:400}"
echo "$diag" | grep -q '"diagnostics":\[' || fail "diagnose missing diagnostics array"
echo "$diag" | grep -Eq '"code":"(type\.index|E0201)"' || fail "diagnose should repeat type.index — ${diag:0:400}"

ok="$("$LIC" check --format=json "$GOOD" 2>&1 || true)"
echo "$ok" | grep -q '"ok":true' || fail "fib should be ok:true — ${ok:0:400}"
echo "$ok" | grep -q '"diagnostics":\[\]' || fail "fib should have empty diagnostics"

warn="$("$LIC" check --format=json "$WARN" 2>&1 || true)"
echo "$warn" | grep -q '"ok":true' || fail "warning-only check should keep ok:true — ${warn:0:400}"
echo "$warn" | grep -q '"severity":"warning"' || fail "expected warning severity: ${warn:0:400}"
echo "$warn" | grep -q '"code":"W0401"' || fail "expected W0401 warning code: ${warn:0:400}"

mixed="$("$LIC" check --format=json "$MIXED" 2>&1 || true)"
echo "$mixed" | grep -q '"ok":false' || fail "mixed severity should be ok:false (errors present): ${mixed:0:400}"
echo "$mixed" | grep -q '"severity":"warning"' || fail "mixed should include warning: ${mixed:0:400}"
echo "$mixed" | grep -q '"severity":"note"' || fail "mixed should include note: ${mixed:0:400}"
echo "$mixed" | grep -q '"severity":"error"' || fail "mixed should include error: ${mixed:0:400}"

if command -v jq >/dev/null 2>&1; then
  echo "$out" | jq -e '.version == 1 and .schema == "diagnostic-v1" and (.diagnostics | length) >= 1' >/dev/null \
    || fail "jq envelope validation failed"
  echo "$out" | "$ROOT/scripts/lic-fix-suggest.sh" >/dev/null 2>&1 || true
fi

echo "diagnose_json_smoke: ok"
