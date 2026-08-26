#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LIC="${LIC:-$ROOT/build/compiler/lic/lic}"
export LI_REPO_ROOT="$ROOT"

LIP="$ROOT/scripts/lip"
LIT="$ROOT/scripts/lit"
if [[ -x "$ROOT/../lip/scripts/lip" ]]; then
  LIP="$ROOT/../lip/scripts/lip"
fi
if [[ -x "$ROOT/../lit/scripts/lit" ]]; then
  LIT="$ROOT/../lit/scripts/lit"
fi
chmod +x "$LIP" "$LIT"

# lip@main pkg_ok fixture may still use proc; lic requires def (see lip#10).
fix_pkg_ok_def_syntax() {
  local d="$ROOT/../lip/fixtures/pkg_ok"
  [[ -d "$d" ]] || return 0
  local f
  for f in "$d/src/lib.li" "$d"/li-tests/smoke/*.li; do
    [[ -f "$f" ]] || continue
    sed 's/^proc /def /g' "$f" > "${f}.tmp" && mv "${f}.tmp" "$f"
  done
}
fix_pkg_ok_def_syntax

# pkg_ok smoke imports pkg_ok_tag; open VC is expected for lip fixture (lip#10).
lic_allow_open_vc_wrap() {
  local wrap="$ROOT/build/lic-allow-open-vc-wrap"
  mkdir -p "$ROOT/build"
  cat >"$wrap" <<WRAP
#!/usr/bin/env bash
set -euo pipefail
real="${LIC}"
if [[ "\${1:-}" == "build" ]]; then
  exec "\$real" build --allow-open-vc "\${@:2}"
fi
exec "\$real" "\$@"
WRAP
  chmod +x "$wrap"
  LIC="$wrap"
  export LIC
}
lic_allow_open_vc_wrap

"$LIP" --version
"$LIT" --version

if [[ -d "$ROOT/../lip/fixtures/pkg_ok" ]]; then
  export LI_REPO_ROOT="$ROOT"
  (cd "$ROOT/../lip/fixtures/pkg_ok" && "$LIT" test --coverage) || {
    echo "lip_lit_smoke: lit failed; reproducing calls_tag:" >&2
    (cd "$ROOT/../lip/fixtures/pkg_ok" && "$LIC" build li-tests/smoke/calls_tag.li -o /dev/null) 2>&1 || true
    exit 1
  }
  (cd "$ROOT/../lip/fixtures/pkg_ok" && "$LIP" publish --dry-run)
else
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  "$LIP" init li-lip-smoke --kind library --out "$TMP/li-lip-smoke"
  test -f "$TMP/li-lip-smoke/li.toml"
  "$LIT" test modules >/dev/null
  "$LIP" lock --out "$TMP/li.lock"
  grep -q proof_digest "$TMP/li.lock"
fi
echo "lip_lit_smoke: ok"
