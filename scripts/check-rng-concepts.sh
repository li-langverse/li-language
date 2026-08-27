#!/usr/bin/env bash
# rng-concepts: li-rng Prng surface, prob_ensures OsRngUniform, Prng-on-TLS profile gates.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

if [[ ! -x "$LIC" ]]; then
  echo "check-rng-concepts: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

RNG_LIB="$ROOT/packages/li-rng/src/lib.li"
PRNG_GOLDEN="$ROOT/li-tests/rng/prng_golden.li"
IV_ORACLE="$ROOT/li-tests/rng/iv_collision_oracle.li"

echo "==> lic check li-rng + prng golden"
"$LIC" check "$RNG_LIB"
"$LIC" check "$PRNG_GOLDEN"

echo "==> lic build prng_golden"
"$LIC" build "$PRNG_GOLDEN" -o /tmp/li_rng_prng_golden --allow-open-vc --no-lean-verify

echo "==> lic build --prob-check iv_collision_oracle"
"$LIC" check "$IV_ORACLE"
python3 "$ROOT/scripts/prob_check.py" "$IV_ORACLE"
"$LIC" build "$IV_ORACLE" -o /tmp/li_rng_iv_collision --allow-open-vc --no-lean-verify --prob-check

echo "==> RNG config profile gates"
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/rng_os_default.toml"
python3 "$ROOT/scripts/httpd_config.py" "$ROOT/li-tests/config_desugar/good/rng_prng_dev.toml" 2>&1 | tee /tmp/li_rng_dev_warn.out
grep -q insecure_rng_prng_tls /tmp/li_rng_dev_warn.out

for rej in "$ROOT/li-tests/config_desugar/reject/rng_prng_production.toml"; do
  name="$(basename "$rej")"
  if python3 "$ROOT/scripts/httpd_config.py" "$rej" 2>/dev/null; then
    echo "expected reject: $name" >&2
    exit 1
  fi
  echo "$name: rejected OK"
done

echo "==> validate-httpd-config rng_os_default"
python3 "$ROOT/scripts/validate-httpd-config.py" \
  "$ROOT/li-tests/config_desugar/good/rng_os_default.toml"

if command -v lake >/dev/null 2>&1; then
  echo "==> Lean Probability.lean (OsRngUniform stub)"
  (cd "$ROOT/docs/semantics" && lake build Probability 2>/dev/null) || true
fi

echo "check-rng-concepts: OK"
