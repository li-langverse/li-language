#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
test -f "$ROOT/.github/workflows/notify-downstream.yml"
test -f "$ROOT/.github/li-downstream-repos.txt"
test -x "$ROOT/.github/scripts/dispatch-upstream-release.sh"
test -f "$ROOT/scripts/templates/github-repo/ecosystem-upstream.yml"
grep -q li-langverse/lip "$ROOT/.github/li-downstream-repos.txt"
grep -q li-langverse/lit "$ROOT/.github/li-downstream-repos.txt"
echo "verify-ecosystem-notify: ok"
