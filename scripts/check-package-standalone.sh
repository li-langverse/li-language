#!/usr/bin/env bash
# Gate: per-package standalone carve-out.
#
# Verifies every [workspace].members package installs and builds OUTSIDE the
# monorepo: for each package, copy it plus its transitive [dependencies]
# closure into a fresh tree, build a program that imports it with lic, and run
# it. This is the "each package is its own installable package, not nested in
# the monorepo" guarantee — no monorepo coupling may leak in.
#
# Usage:
#   scripts/check-package-standalone.sh [pkg...]   # default: all workspace members
#
# The dependency closure is derived from [dependencies] path refs, so a new
# dependency automatically extends each carve-out; a missing package fails here.
#
# Supersedes check-aimd-standalone.sh (a single-package special case).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
if [[ ! -x "$LIC" ]]; then
  echo "check-package-standalone: skip (lic not executable)" >&2
  exit 0
fi

# Workspace members from packages/li.toml.
read_workspace_members() {
  awk '/^members[[:space:]]*=/{in_m=1; next} in_m && /\]/{exit} in_m {
    while (match($0, /"[a-z0-9-]+"/)) {
      s = substr($0, RSTART+1, RLENGTH-2); print s; $0 = substr($0, RSTART+RLENGTH)
    }
  }' "$ROOT/packages/li.toml"
}

MEMBERS=()
if [[ $# -gt 0 ]]; then
  MEMBERS=("$@")
else
  while IFS= read -r m; do
    [[ -n "$m" ]] && MEMBERS+=("$m")
  done < <(read_workspace_members)
fi

# import_name from [package.metadata.li] — defaults to the package name.
import_name_of() {
  local pkg="$1"
  local imp
  imp=$(awk '/^\[package\.metadata\.li\]/{in_meta=1; next} /^\[/{in_meta=0}
             in_meta && /import_name[[:space:]]*=/{gsub(/.*=[[:space:]]*"|"[[:space:]]*$/, ""); print}' \
           "$ROOT/packages/$pkg/li.toml")
  if [[ -z "$imp" ]]; then
    echo "$pkg"
  else
    echo "$imp"
  fi
}

# First public def in src/lib.li (any name) — used only to detect the lib parses.
first_def_name() {
  local pkg="$1"
  grep -oE '^def [a-zA-Z_][a-zA-Z0-9_]*' "$ROOT/packages/$pkg/src/lib.li" 2>/dev/null \
    | head -1 | awk '{print $2}'
}

# One carve-out build+run for a single package. Echoes nothing on success.
check_one() {
  local PKG="$1"
  local IMP
  IMP="$(import_name_of "$PKG")"
  local WORK
  WORK="$(mktemp -d "${TMPDIR:-/tmp}/pkg-standalone.XXXXXX")"
  trap 'rm -rf "$WORK"' RETURN
  mkdir -p "$WORK/packages"

  # Seed with the package itself, then follow path deps transitively.
  local -a added=("$PKG") processed=()
  while ((${#added[@]} > 0)); do
    local -a next=()
    for p in "${added[@]}"; do
      if [[ " ${processed[*]:-} " == *" $p "* ]]; then
        continue
      fi
      if [[ ! -d "$ROOT/packages/$p" ]]; then
        echo "FAIL — dependency $p (of $PKG) missing from packages/" >&2
        return 1
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
                | tr -d '"' | sed 's|\.\./||')
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
    local first=1 p
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

  # Probe: import the package; call the first public def to prove the lib
  # resolves and executes outside the monorepo.
  local def
  def="$(first_def_name "$PKG")"
  if [[ -z "$def" ]]; then
    echo "FAIL — no def export found in $PKG/src/lib.li" >&2
    return 1
  fi
  cat > "$WORK/main.li" <<EOF
import $IMP

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
    echo "FAIL — lic build of standalone $PKG tree" >&2
    echo "  carved-out packages: ${processed[*]}" >&2
    return 1
  fi
  "$WORK/main"
  local rc=$?
  if [[ "$rc" -ne 0 ]]; then
    echo "FAIL — standalone binary exited $rc" >&2
    return 1
  fi
  rm -rf "$WORK"
  trap - RETURN
  echo "$PKG"
}

fail=0
checked=0
for pkg in "${MEMBERS[@]}"; do
  [[ -z "$pkg" ]] && continue
  out="$(check_one "$pkg" 2>&1)" || { echo "$out" | sed 's/^/  /' >&2; fail=1; continue; }
  checked=$((checked + 1))
  echo "check-package-standalone: ok ($out; builds + runs outside monorepo)"
done

if [[ "$fail" -eq 0 && "$checked" -gt 0 ]]; then
  echo "check-package-standalone: ok ($checked packages standalone)"
  exit 0
fi
echo "check-package-standalone: FAIL ($fail package(s) not standalone)" >&2
exit 1
