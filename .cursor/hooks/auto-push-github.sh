#!/usr/bin/env bash
# Cursor stop hook — push to GitHub when agent finishes (user rule: no manual push).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ "${LI_SKIP_AUTO_PUSH:-}" == "1" ]]; then
  exit 0
fi
if [[ ! -x "$ROOT/scripts/agent-push-github.sh" ]]; then
  exit 0
fi
# Best-effort: do not fail the session if push fails (token/network).
if "$ROOT/scripts/agent-push-github.sh" "chore(agent): session sync" 2>&1; then
  exit 0
fi
echo "auto-push-github: push failed (check ../.env.github GH_TOKEN)" >&2
exit 0
