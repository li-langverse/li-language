#!/usr/bin/env bash
# Cursor stop — suggest verification if sensitive paths were in session (informational).
set -euo pipefail
echo "session end: if you touched std/, compiler/, or security/, run ./scripts/ci.sh; push via ./scripts/agent-push-github.sh (see li-auto-push rule)" >&2
exit 0
