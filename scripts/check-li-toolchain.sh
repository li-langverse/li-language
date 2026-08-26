#!/usr/bin/env bash
# Warn if li-toolchain.toml pins are missing; optional compare to latest lic release (needs gh).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0
LIC_ORG_REPO="${LIC_ORG_REPO:-li-langverse/lic}"

check_one() {
  local f="$1"
  if [[ ! -f "$f" ]]; then
    echo "WARN missing $f"
    [[ "${CHECK_LI_TOOLCHAIN_STRICT:-0}" == "1" ]] && FAIL=1
    return
  fi
  if ! grep -q 'lic_version' "$f" 2>/dev/null; then
    echo "WARN $f: no lic_version pin"
    [[ "${CHECK_LI_TOOLCHAIN_STRICT:-0}" == "1" ]] && FAIL=1
  fi
}

if [[ -f "$ROOT/li-toolchain.toml" ]]; then
  check_one "$ROOT/li-toolchain.toml"
fi
for f in "$ROOT"/packages/*/li-toolchain.toml; do
  [[ -f "$f" ]] || continue
  check_one "$f"
done

if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1 && [[ -f "$ROOT/li-toolchain.toml" ]]; then
  LATEST="$(gh release view -R "$LIC_ORG_REPO" --json tagName -q .tagName 2>/dev/null || true)"
  PINNED="$(grep -E 'lic_version' "$ROOT/li-toolchain.toml" | head -1 | sed -n 's/.*"\([^"]*\)".*/\1/p')"
  if [[ -n "$LATEST" && -n "$PINNED" ]]; then
    if printf '%s\n' "${PINNED#v}" "${LATEST#v}" | sort -V | head -1 | grep -qx "${PINNED#v}" && [[ "${PINNED#v}" != "${LATEST#v}" ]]; then
      echo "WARN lic_version=${PINNED} older than ${LIC_ORG_REPO} latest ${LATEST}"
      [[ "${CHECK_LI_TOOLCHAIN_STRICT:-0}" == "1" ]] && FAIL=1
    fi
  fi
else
  echo "note: gh auth optional — compares pins to ${LIC_ORG_REPO} releases"
fi

[[ "$FAIL" -eq 0 ]] || exit 1
echo "check-li-toolchain: ok"
