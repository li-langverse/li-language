# Phase H — li-http package + std.bytes import fix + P0 runtime

## Summary

Adds workspace package **`li-http`** (`import http`) with `parse_request` and a GET method-line probe, ships P0 **`li_rt_net`** TCP stubs and **`bytes_len`/`bytes_slice`** runtime, fixes **`std.bytes` / `std.csv`** single-segment import paths, and tightens workspace **`members`** TOML parsing plus import failure diagnostics.

## Agent continuation

1. Read `docs/ecosystem/httpd-prerequisites.md` for remaining P0 rows (epoll, full HTTP header FSM, strict Lean).
2. Run `export CC=clang-22 LI_REPO_ROOT=$PWD && cmake --build build && ./li-tests/run_all.sh runtime composable httpd && ./scripts/lic-workspace-build.sh`.
3. Next: header field parsing + `Content-Length` duplicate detection in `packages/li-http`; keep contracts int-first.
4. Blocked: production sockets until trusted-net RFC + real `li_rt_net` syscalls.

## Changed

- `compiler/types/import_resolve.cpp` — `std_module_to_path` single-segment; workspace `members` bracket depth; absolute importer; unresolved import errors.
- `compiler/mir/lower.cpp`, `compiler/codegen/emit.cpp`, `compiler/codegen/compile.cpp` — std extern MIR + LLVM decls + `li_rt_net.c` link.
- `runtime/li_rt.c`, `runtime/li_rt.h` — `bytes_len`, `bytes_slice`, `li_rt_str_byte_at`.
- `runtime/li_rt_net.c`, `runtime/li_rt_net.h` — TCP stubs.
- `packages/li-http/**` — new package; `packages/li.toml` — workspace member.
- `packages/li-net-httpd/**` — deps + `httpd_serve` / `httpd_tag` / `httpd_stop`.
- `li-tests/**` — runtime link tests, `parse_request_smoke`, `import_http_lib`, manifest; `run_all.sh` exports `LI_REPO_ROOT`.
- `security/trusted-c-audit.toml`, `docs/ecosystem/httpd-prerequisites.md`, `CHANGELOG.md`.

## Not changed

- Full HTTP/1.1 state machine, chunked encoding, HTTP/2, TLS.
- `lic verify --strict-lean` default-on policy.

## Breaking

| Before | After |
|--------|--------|
| Broken `import std.bytes` / `import std.csv` path | Resolves to `std/<seg>/<seg>.li` |
| Silent skip of unresolved non-`std` imports | **Error** at resolve time |

## Security

| Item | Evidence |
|------|----------|
| Trusted C | `security/trusted-c-audit.toml` rows for `li_rt_net.c`, `li_rt_str_byte_at`, `bytes_slice` |

## Performance

N/A — stub network; no bench threshold changes.

## Downstream

- Pin `lic` in `lis` / `benchmarks` after merge; run `./scripts/ingest/ingest-lic.sh` only if catalog paths change.
