#!/usr/bin/env bash
# C-level oracle for match_route_fixture (same cases as routing/cases/api_prefix.toml).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-clang}"
mkdir -p "$ROOT/build"
RT_O="$ROOT/build/httpd_li_rt.o"
BIN="$ROOT/build/httpd_route_fixture_test"
SRC="$ROOT/build/httpd_route_fixture_main.c"

cat >"$SRC" <<'EOF'
#include <stdint.h>
extern int32_t li_rt_match_route_fixture(const char* method, const char* path);
int main(void) {
  if (li_rt_match_route_fixture("GET", "/health") != 1) {
    return 1;
  }
  if (li_rt_match_route_fixture("POST", "/v1/chat/completions") != 2) {
    return 2;
  }
  if (li_rt_match_route_fixture("GET", "/unknown") != 0) {
    return 3;
  }
  return 0;
}
EOF

"$CC" -c "$ROOT/runtime/li_rt.c" -o "$RT_O"
"$CC" -c "$SRC" -o "$ROOT/build/httpd_route_fixture_main.o"
"$CC" "$RT_O" "$ROOT/build/httpd_route_fixture_main.o" -lm -o "$BIN"
"$BIN"
echo "check-httpd-route-fixture: OK"
