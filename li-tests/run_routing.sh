#!/usr/bin/env bash
# M1 routing: table cases (Python match_route) + overlap config_reject + Li oracles.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${ROOT}/scripts"
export PYTHONPATH="$PY${PYTHONPATH:+:$PYTHONPATH}"
ROUTING_CFG="$ROOT/li-tests/httpd/fixtures/routing.toml"

echo "== routing overlap (same priority must reject) =="
python3 "$PY/check-httpd-overlap-reject.py"

echo "== routing config_reject (overlap) =="
for rej in "$ROOT/li-tests/config_reject"/routing_*.toml; do
  [[ -f "$rej" ]] || continue
  name="$(basename "$rej")"
  if python3 "$PY/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected routing reject to fail: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

echo "== routing cases (Python oracle) =="
for cases in "$ROOT/li-tests/routing/cases"/*.toml; do
  python3 "$PY/httpd_match.py" "$ROUTING_CFG" "$cases"
done

echo "== routing (Li compile + binary oracle) =="
if [[ "${HTTPD_GATES_SKIP_LIC_BUILD:-0}" == "1" ]]; then
  echo "skip Li routing compile (HTTPD_GATES_SKIP_LIC_BUILD=1)"
  echo "run_routing: OK"
  exit 0
fi
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
HTTPD_BUILD_FLAGS=(--allow-open-vc)
export LI_REPO_ROOT="$ROOT"
"$LIC" build "${HTTPD_BUILD_FLAGS[@]}" "$ROOT/li-tests/routing/match_routes.li" -o /tmp/li_match_routes
if [[ "${HTTPD_SKIP_LI_ROUTING_BIN:-0}" != "1" ]]; then
  /tmp/li_match_routes
  test "$(/tmp/li_match_routes; echo $?)" -eq 0
else
  echo "skip Li routing binary run (HTTPD_SKIP_LI_ROUTING_BIN=1)"
fi

"$LIC" build "${HTTPD_BUILD_FLAGS[@]}" "$ROOT/li-tests/routing/match_routes_toml.li" -o /tmp/li_match_routes_toml
if [[ "${HTTPD_SKIP_LI_ROUTING_BIN:-0}" != "1" ]]; then
  /tmp/li_match_routes_toml
  test "$(/tmp/li_match_routes_toml; echo $?)" -eq 0
fi

echo "run_routing: OK"
