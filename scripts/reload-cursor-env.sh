#!/usr/bin/env bash
# Reload Cursor API credentials for httpd-plan-loop / li-cursor-agents.
# Cloud Agent VMs inject secrets at start; after dashboard updates, restart the VM
# or run this script to drop stale misleading vars (e.g. CURSOR_SDK=dashboard URL).
set -euo pipefail

if [[ -n "${CURSOR_SDK:-}" ]] && [[ "${CURSOR_SDK}" == http* ]]; then
  echo "reload-cursor-env: unsetting CURSOR_SDK (was dashboard URL, not an API key)" >&2
  unset CURSOR_SDK
fi

export LI_CONTROL_PLANE_STORE="${LI_CONTROL_PLANE_STORE:-disk}"

if [[ -z "${CURSOR_API_KEY:-}" ]]; then
  echo "reload-cursor-env: CURSOR_API_KEY is not set" >&2
  echo "  Add it in Cursor → Cloud Agents → Secrets, then restart this Cloud Agent." >&2
  exit 1
fi

python3 - <<'PY'
import os, urllib.request, base64
key = os.environ.get("CURSOR_API_KEY", "").strip()
auth = base64.b64encode(f"{key}:".encode()).decode()
req = urllib.request.Request(
    "https://api.cursor.com/v1/me",
    headers={"Authorization": f"Basic {auth}", "Accept": "application/json"},
)
try:
    with urllib.request.urlopen(req, timeout=15) as r:
        print("reload-cursor-env: API ok (HTTP", r.status, ")")
except urllib.error.HTTPError as e:
    print("reload-cursor-env: API probe failed HTTP", e.code, file=__import__("sys").stderr)
    print("  If you just updated Secrets, restart the Cloud Agent VM so injection refreshes.", file=__import__("sys").stderr)
    print("  Mint key: https://cursor.com/dashboard → Integrations (Cloud Agents).", file=__import__("sys").stderr)
    raise SystemExit(1)
PY

echo "reload-cursor-env: ready (LI_CONTROL_PLANE_STORE=$LI_CONTROL_PLANE_STORE)"
