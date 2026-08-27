#!/usr/bin/env bash
# w0-bytes-io: std bytes/stringview/Reader/Writer + raises Net + trusted-net RFC exit gates.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

if [[ ! -x "$LIC" ]]; then
  echo "check-w0-bytes-io: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

BYTES="$ROOT/std/bytes/bytes.li"
RFC="$ROOT/docs/superpowers/specs/2026-05-16-li-trusted-net-rfc.md"
TRUSTED="$ROOT/docs/semantics/trusted.lean"

echo "==> RFC accepted"
grep -q '^\*\*Status:\*\* accepted' "$RFC" || {
  echo "check-w0-bytes-io: trusted-net RFC must be Status: accepted" >&2
  exit 1
}

echo "==> trusted.lean Net axioms"
grep -q 'axiom Net' "$TRUSTED"
grep -q 'tcp_listen_stub' "$TRUSTED"

echo "==> lic check std.bytes"
"$LIC" check "$BYTES"

echo "==> lic build bytes smokes"
"$LIC" build "$ROOT/li-tests/runtime/bytes_len_slice_link.li" \
  -o /tmp/li_bytes_len_slice_gate --allow-open-vc --no-lean-verify
echo "==> Reader/Writer API (check-only; object+bytes ABI follow-up)"
"$LIC" check "$ROOT/li-tests/bytes/reader_writer_smoke.li"

echo "==> raises Net policy"
"$LIC" build "$ROOT/li-tests/effects/net_ok.li" -o /tmp/li_net_ok --allow-open-vc --no-lean-verify
"$LIC" build "$ROOT/li-tests/effects/net_forward_ok.li" -o /tmp/li_net_fwd --allow-open-vc --no-lean-verify
"$LIC" build "$ROOT/li-tests/runtime/tcp_stub_link.li" -o /tmp/li_tcp_stub --allow-open-vc --no-lean-verify
"$LIC" build "$ROOT/li-tests/net_trusted/seam_policy_ok.li" \
  -o /tmp/li_net_seam_ok --allow-open-vc --no-lean-verify

if "$LIC" build "$ROOT/li-tests/effects/net_missing_raises.li" -o /tmp/li_net_bad 2>/dev/null; then
  echo "check-w0-bytes-io: expected compile_fail net_missing_raises" >&2
  exit 1
fi
if "$LIC" build "$ROOT/li-tests/net_trusted/seam_missing_net.li" -o /tmp/li_net_seam_bad 2>/dev/null; then
  echo "check-w0-bytes-io: expected compile_fail seam_missing_net" >&2
  exit 1
fi

echo "==> li-bytes package smoke"
"$LIC" check "$ROOT/packages/li-bytes/src/lib.li"
"$LIC" build "$ROOT/packages/li-bytes/li-tests/smoke/builds.li" \
  -o /tmp/li_bytes_pkg --allow-open-vc --no-lean-verify

echo "check-w0-bytes-io: OK"
