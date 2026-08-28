#!/usr/bin/env bash
# Gate: aimd standalone carve-out.
#
# Verifies li-aimd installs and builds OUTSIDE the monorepo: copy the package
# plus its transitive [dependencies] closure into a fresh tree, build a program
# that imports it with lic, and run it. This is the "aimd is its own package,
# installable by itself" guarantee — no monorepo coupling may leak in.
#
# The dependency closure is derived from [dependencies] path refs, so a new
# dependency automatically extends the carve-out; a missing package fails here.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
if [[ ! -x "$LIC" ]]; then
  echo "check-aimd-standalone: skip (lic not executable)" >&2
  exit 0
fi

PKG="li-aimd"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/aimd-standalone.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/packages"

# Seed with the package itself, then follow path deps transitively.
added=("$PKG")
processed=()
while ((${#added[@]} > 0)); do
  next=()
  for p in "${added[@]}"; do
    if [[ " ${processed[*]:-} " == *" $p "* ]]; then
      continue
    fi
    if [[ ! -d "$ROOT/packages/$p" ]]; then
      echo "check-aimd-standalone: FAIL — package $p missing from packages/" >&2
      exit 1
    fi
    cp -R "$ROOT/packages/$p" "$WORK/packages/"
    processed+=("$p")
    while IFS= read -r dep; do
      [[ -n "$dep" ]] && next+=("$dep")
    done < <(awk '/^\[dependencies\]/{in_dep=1; next} /^\[/{in_dep=0}
                in_dep && /^[[:space:]]*#/{next}
                in_dep && /path[[:space:]]*=/{print}' \
              "$ROOT/packages/$p/li.toml" \
              | grep -oE '"\.\./[a-z0-9-]+"' \
              | tr -d '""' | sed 's|\.\./||')
  done
  if (( ${#next[@]} > 0 )); then
    added=("${next[@]}")
  else
    added=()
  fi
done

{
  echo "[workspace]"
  printf 'members = ['
  first=1
  for p in "${processed[@]}"; do
    if [[ "$first" -eq 1 ]]; then
      printf '"%s"' "$p"
      first=0
    else
      printf ', "%s"' "$p"
    fi
  done
  echo ']'
  echo 'resolver = "path"'
} > "$WORK/packages/li.toml"

# Probe: import the package and call one of its exports.
EXPORT="$(grep -oE '^def aimd_[a-z0-9_]+' "$ROOT/packages/$PKG/src/lib.li" | head -1 | awk '{print $2}')"
if [[ -z "$EXPORT" ]]; then
  echo "check-aimd-standalone: FAIL — no aimd_* export found in src/lib.li" >&2
  exit 1
fi
cat > "$WORK/main.li" <<EOF
import aimd

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var v: int = 0
  return v
EOF

if ! (cd "$ROOT" && LI_REPO_ROOT="$WORK" "$LIC" build --allow-open-vc --no-lean-verify \
      "$WORK/main.li" -o "$WORK/main" >/dev/null 2>&1); then
  echo "check-aimd-standalone: FAIL — lic build of standalone li-aimd tree" >&2
  echo "  carved-out packages: ${processed[*]}" >&2
  exit 1
fi
"$WORK/main"
rc=$?
if [[ "$rc" -ne 0 ]]; then
  echo "check-aimd-standalone: FAIL — standalone binary exited $rc" >&2
  exit 1
fi
echo "check-aimd-standalone: ok (carve-out: ${processed[*]}; builds + runs outside monorepo)"
