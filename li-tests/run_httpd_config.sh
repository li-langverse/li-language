#!/usr/bin/env bash
# M1 prep: easy TOML desugar, validate, routing cases (Python oracle until li-httpd ships).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${ROOT}/scripts"
export PYTHONPATH="$PY${PYTHONPATH:+:$PYTHONPATH}"

echo "== httpd config desugar (good) =="
for f in "$ROOT/li-tests/config_desugar/good"/*.toml; do
  python3 "$PY/httpd_config.py" "$f"
done

echo "== httpd m15 config =="
chmod +x "$ROOT/scripts/check-httpd-m15-config.sh"
"$ROOT/scripts/check-httpd-m15-config.sh"

echo "== httpd leak_censor (M1.5) =="
chmod +x "$ROOT/scripts/check-httpd-leak-censor.sh"
"$ROOT/scripts/check-httpd-leak-censor.sh"

echo "== httpd tls auto (M1.5) =="
chmod +x "$ROOT/scripts/check-httpd-tls-auto.sh" "$ROOT/li-tests/tls_setup/run_setup_tls.sh"
"$ROOT/scripts/check-httpd-tls-auto.sh"

echo "== httpd config reject =="
for rej in "$ROOT/li-tests/config_desugar/reject"/*.toml; do
  name="$(basename "$rej")"
  if python3 "$PY/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject to fail: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

export LI_REPO_ROOT="$ROOT"
chmod +x "$ROOT/li-tests/run_routing.sh"
"$ROOT/li-tests/run_routing.sh"

if [[ "${HTTPD_GATES_SKIP_LIC_BUILD:-0}" == "1" ]]; then
  echo "skip lic CLI checks (HTTPD_GATES_SKIP_LIC_BUILD=1)"
  echo "run_httpd_config: OK"
  exit 0
fi
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

echo "== routing (Li serve_routed_once oracle) =="
HTTPD_BUILD_FLAGS=(--allow-open-vc)
if [[ "${HTTPD_SKIP_SERVE_ROUTED_ONCE:-1}" != "1" ]]; then
  "$LIC" build "${HTTPD_BUILD_FLAGS[@]}" "$ROOT/li-tests/httpd/serve_routed_once.li" -o /tmp/li_serve_routed_once
  if [[ "${HTTPD_SKIP_LI_ROUTING_BIN:-0}" != "1" ]]; then
    /tmp/li_serve_routed_once
    test "$(/tmp/li_serve_routed_once; echo $?)" -eq 0
  fi
else
  echo "skip serve_routed_once (HTTPD_SKIP_SERVE_ROUTED_ONCE=1)"
fi

echo "== validate-config (lic CLI) =="
"$LIC" httpd validate-config "$ROOT/li-tests/config_desugar/good/agent_gateway.toml"
for rej in "$ROOT/li-tests/config_desugar/reject"/*.toml; do
  name="$(basename "$rej")"
  # M1.5/TOML policy rejects are covered by dedicated Python gate scripts, not C E0501–E0504.
  case "$name" in
    leak_censor_bad_pattern.toml \
    | m15_inference_no_traceparent.toml \
    | m15_missing_stream_limits.toml \
    | m2_http2_no_tls.toml \
    | m2_queue_depth_excess.toml \
    | m2_webhook_private_ip.toml \
    | m3_l4_no_upstream.toml \
    | m3_l4_private_upstream.toml \
    | m3_token_budget_bad_header.toml \
    | m3_token_cap_excess.toml \
    | rng_prng_production.toml \
    | tls_le_missing_email.toml \
    | tls_public_no_tls.toml \
    | tls_public_self_signed.toml)
      continue
      ;;
  esac
  if "$LIC" httpd validate-config "$rej" 2>/dev/null; then
    echo "validate-config: expected reject for $name" >&2
    exit 1
  fi
  echo "validate-config $name: rejected OK"
done

echo "== validate-httpd-config (Python M1 schema) =="
"$ROOT/scripts/lic-validate-httpd-config.sh" "$ROOT/packages/li-net-httpd/examples/auth_bearer.toml"
"$ROOT/scripts/lic-validate-httpd-config.sh" "$ROOT/packages/li-net-httpd/examples/agent_gateway_limits.toml"
for rej in "$ROOT/li-tests/config_desugar/reject"/proxy_without_rate_limit.toml; do
  [[ -f "$rej" ]] || continue
  if "$ROOT/scripts/lic-validate-httpd-config.sh" "$rej" 2>/dev/null; then
    echo "validate-httpd-config: expected reject for $(basename "$rej")" >&2
    exit 1
  fi
  echo "validate-httpd-config $(basename "$rej"): rejected OK"
done

echo "== explain-config (lic CLI + C/Python parity) =="
export LI_REPO_ROOT="$ROOT"
for cfg in "$ROOT/li-tests/config_desugar/good"/*.toml; do
  base="$(basename "$cfg" .toml)"
  golden="$ROOT/li-tests/config_desugar/good/${base}.explained.golden"
  if [[ ! -f "$golden" ]]; then
    echo "skip explain-config golden for $base (no ${base}.explained.golden)"
    continue
  fi
  "$LIC" httpd explain-config "$cfg" | diff -u "$golden" -
done
chmod +x "$ROOT/scripts/check-httpd-config-desugar.sh"
"$ROOT/scripts/check-httpd-config-desugar.sh"

echo "== httpd LB mode smoke (Li) =="
HTTPD_BUILD_FLAGS=(--allow-open-vc)
"$LIC" build "${HTTPD_BUILD_FLAGS[@]}" "$ROOT/li-tests/httpd/lb_mode_smoke.li" -o /tmp/li_lb_mode_smoke
if [[ "${HTTPD_SKIP_LI_ROUTING_BIN:-0}" != "1" ]]; then
  /tmp/li_lb_mode_smoke
  test "$(/tmp/li_lb_mode_smoke; echo $?)" -eq 0
fi

echo "run_httpd_config: OK"
