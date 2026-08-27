#!/usr/bin/env bash
# Cursor beforeShellExecution — block backup scripts and backup env (human-operated).
set -euo pipefail
input="$(cat)"
cmd="$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('command',''))" 2>/dev/null || true)"
[[ -n "$cmd" ]] || exit 0
if [[ "${LI_HOOK_ALLOW:-}" == "1" ]]; then
  exit 0
fi
if echo "$cmd" | grep -qE 'backup-li-langverse-org|export-li-langverse-metadata|recovery-drill-li-langverse|with-github-backup-env'; then
  echo "blocked: org backup scripts are human-operated (set LI_HOOK_ALLOW=1 if intentional)" >&2
  exit 2
fi
exit 0
