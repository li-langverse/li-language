#!/usr/bin/env bash
# Drift check: the stdlib-seal name lists the Li walker carries in
# bootstrap/lic/main.li mirror tables that the C++ reference owns —
#   seal_numeric_scalar <- compiler/types/numeric_types.cpp `table()`
#   seal_prelude_type   <- compiler/types/prelude.cpp `is_prelude_type_name`
#   seal_prelude_proc   <- compiler/types/prelude.cpp `is_prelude_proc_name`
#   seal_std_symbol     <- compiler/types/prelude.cpp `is_std_module_symbol`
# A one-name edit on either side silently changes which programs the two
# compilers accept, so the walker's copy is compared entry by entry instead of
# trusted. The reference keeps its own std-export list by hand too
# (scripts/gen-stdlib-manifest.sh prints the std/**/*.li names to sync against);
# this check pins the walker to whatever the reference ends up declaring.
#
# Usage: scripts/check-seal-name-sync.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MAIN="$ROOT/bootstrap/lic/main.li"
NUMERIC="$ROOT/compiler/types/numeric_types.cpp"
PRELUDE="$ROOT/compiler/types/prelude.cpp"

# Numeric scalar aliases: the `add_int|add_uint|add_float("alias"` rows of the
# numeric type table.
cpp_numeric_names() {
  grep -oE 'add_(int|uint|float)\("[^"]*"' "$NUMERIC" | sed 's/.*("//; s/"$//' | sort -u
}

# One `in_set(name, {...})` initializer from prelude.cpp; the quoted-name match
# drops the `nullptr` terminator, and the walk starts only at the named
# function so an earlier brace cannot open the list early.
cpp_in_set_names() {
  awk -v fn="bool $1(" 'index($0, fn) == 1 {fn_found = 1}
       fn_found && /in_set\(name, \{/ {in_list = 1}
       in_list {print}
       in_list && /\}/ {exit}' "$PRELUDE" \
    | grep -o '"[^"]*"' | tr -d '"' | sort -u
}

# The string literals inside one `def <name>(` body on the walker side; empty
# strings are the padding slots seal_any4 ignores.
li_names() {
  awk -v fn="$1" '$0 ~ "^def " fn "\\(" {in_body = 1}
       in_body && /^def / && $0 !~ "^def " fn "\\(" {exit}
       in_body' "$MAIN" \
    | grep -o '"[^"]*"' | tr -d '"' | grep -v '^$' | sort -u
}

fail=0

compare() {
  local label="$1" owner="$2" walker="$3" name
  if [[ "$owner" == "$walker" ]]; then
    printf '  ok     %-24s %2s names\n' "$label" "$(printf '%s\n' "$walker" | grep -c .)"
    return
  fi
  fail=1
  while IFS= read -r name; do
    [[ -n "$name" ]] && printf '  DRIFT  %-24s reference only: %s\n' "$label" "$name"
  done < <(comm -23 <(printf '%s\n' "$owner") <(printf '%s\n' "$walker"))
  while IFS= read -r name; do
    [[ -n "$name" ]] && printf '  DRIFT  %-24s walker only:    %s\n' "$label" "$name"
  done < <(comm -13 <(printf '%s\n' "$owner") <(printf '%s\n' "$walker"))
}

compare "numeric scalar aliases" "$(cpp_numeric_names)" \
  "$(li_names seal_numeric_scalar)"
compare "prelude type names" "$(cpp_in_set_names is_prelude_type_name)" \
  "$(li_names seal_prelude_type)"
compare "prelude proc names" "$(cpp_in_set_names is_prelude_proc_name)" \
  "$(li_names seal_prelude_proc)"
compare "std export symbols" "$(cpp_in_set_names is_std_module_symbol)" \
  "$(li_names seal_std_symbol)"

if [[ "$fail" -ne 0 ]]; then
  echo "check-seal-name-sync: FAIL — bootstrap/lic/main.li drifted from the reference seal tables" >&2
  exit 1
fi
echo "check-seal-name-sync: ok (4 seal lists match compiler/types/{numeric_types,prelude}.cpp)"
