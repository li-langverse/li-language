#!/usr/bin/env bash
# Emit fuzz workflow status for Obs dashboards (Phase Obs).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WF="$ROOT/.github/workflows/fuzz.yml"
if [[ ! -f "$WF" ]]; then
  echo "fuzz: missing workflow"
  exit 1
fi
echo "fuzz_workflow=present"
grep -E '^\s+schedule:' -A2 "$WF" || true
echo "corpus_dir=compiler/fuzz/corpus"
find "$ROOT/compiler/fuzz/corpus" -type f 2>/dev/null | wc -l | awk '{print "corpus_files="$1}'
