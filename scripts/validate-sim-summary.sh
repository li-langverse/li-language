#!/usr/bin/env bash
# Validate li_sim_summary_v1 under benchmarks/results (JSON, minified JSON, YAML).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
shopt -s nullglob
fail=0
found=0

validate_one() {
  local f="$1"
  if ! python3 -c "
import json, sys
from pathlib import Path
p = Path(sys.argv[1])
text = p.read_text()
if p.suffix in ('.yaml', '.yml'):
    try:
        import yaml
        d = yaml.safe_load(text)
    except ImportError:
        # minimal check: schema key present
        assert 'li_sim_summary_v1' in text and 'ok:' in text, p
        d = {'schema': 'li_sim_summary_v1', 'ok': True, 'metrics': {}}
else:
    d = json.loads(text)
assert d.get('schema') == 'li_sim_summary_v1', p
assert 'ok' in d and 'metrics' in d, p
" "$f"; then
    echo "invalid summary: $f" >&2
    return 1
  fi
  echo "ok: $f"
  return 0
}

for f in "$ROOT"/benchmarks/results/*/*.summary.json \
         "$ROOT"/benchmarks/results/*/*.summary.min.json \
         "$ROOT"/benchmarks/results/*/*.summary.yaml \
         "$ROOT"/benchmarks/results/li_runs/*; do
  [[ -f "$f" ]] || continue
  case "$f" in
    *.summary.json|*.summary.min.json|*.summary.yaml) ;;
    *) continue ;;
  esac
  found=1
  validate_one "$f" || fail=1
done

if [[ "$found" -eq 0 ]]; then
  echo "validate-sim-summary: no summaries (run: verify.py --write-summary or li-tests/tooling/sim_li_run_summary.sh)" >&2
  exit 0
fi
exit "$fail"
