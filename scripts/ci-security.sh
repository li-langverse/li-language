#!/usr/bin/env bash
# CVE + malformed-input security gates — same on Linux, macOS, and Windows (Git Bash).
# Sanitizer-only jobs (ASan/UBSan/fuzz) remain OS-specific; see docs/testing/security.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

if [[ -z "${LIC:-}" ]]; then
  LIC="$("$ROOT/scripts/resolve-lic.sh")"
  export LIC
fi

if [[ ! -x "$LIC" ]]; then
  echo "ci-security: skip (lic not executable at $LIC)"
  exit 0
fi

li_phase "security corpus"
chmod +x "$ROOT/li-tests/run_security.sh"
"$ROOT/li-tests/run_security.sh"

li_phase "stdlib_seal"
"$ROOT/li-tests/run_all.sh" stdlib_seal

li_phase "cve_patterns"
"$ROOT/li-tests/run_all.sh" cve_patterns

li_phase "CVE catalog coverage"
chmod +x "$ROOT/scripts/check-cve-coverage.sh"
"$ROOT/scripts/check-cve-coverage.sh"

li_phase "historic bug registry"
chmod +x "$ROOT/scripts/check-historic-bugs.sh"
"$ROOT/scripts/check-historic-bugs.sh"

li_phase "webserver vulnerability registry"
chmod +x "$ROOT/scripts/check-webserver-bugs.sh"
"$ROOT/scripts/check-webserver-bugs.sh"

li_phase "codegen path injection"
chmod +x "$ROOT/li-tests/security/codegen_path_injection.sh"
"$ROOT/li-tests/security/codegen_path_injection.sh"

li_gate_ok "security ($(uname -s 2>/dev/null || echo unknown))"
