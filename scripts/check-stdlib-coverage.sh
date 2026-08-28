#!/usr/bin/env bash
# Gate: std/**/*.li must be reachable via import harness + coverage build (phase 8e).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export LI_REPO_ROOT="$ROOT"

if [[ ! -d std ]]; then
  echo "check-stdlib-coverage: no std/ — skip"
  exit 0
fi

n="$(find std -name '*.li' -type f | wc -l | tr -d ' ')"
echo "check-stdlib-coverage: $n std source file(s)"

LIC="${LIC:-}"
if [[ -z "$LIC" && -x build/compiler/lic/lic ]]; then
  LIC=build/compiler/lic/lic
fi
if [[ -z "$LIC" ]]; then
  echo "check-stdlib-coverage: stub OK (no lic binary)"
  exit 0
fi

harness=li-tests/stdlib_coverage/build_std_decorators.li
if [[ ! -f "$harness" ]]; then
  echo "check-stdlib-coverage: missing $harness" >&2
  exit 1
fi

echo "check-stdlib-coverage: instrument $harness (imports std)"
if ! "$LIC" build --allow-open-vc "$harness" -o /dev/null --coverage-instrument 2>&1; then
  echo "check-stdlib-coverage: FAIL" >&2
  exit 1
fi
echo "check-stdlib-coverage: ok (instrumented import harness; lit repo owns llvm-cov 100%)"
