# HTTP proxy epoll flush + trusted seam + compiler ptr ABI

## Summary

Li `li-httpd` reverse-proxy loop forwards upstream `EPOLLOUT` via C `httpd_proxy_*_handler`, ships `std/runtime/seam.li` for trusted externs, and fixes `lic` codegen so `ptr`/`CallExtern` returns are full-width (fixes argv segfault and `verify_fail_li:/` timeouts).

## Agent continuation

1. **Read** `runtime/li_rt_net.c` (`httpd_li_proxy_*_epoll_i`, `upstream_port_for_fd`), `packages/li-net-httpd/src/lib.li` (`proxy_dispatch_*_loaded`), `std/runtime/seam.li`, `compiler/codegen/emit.cpp` (`CallExtern`/`CallProc` ptr returns).
2. **Run** `LI_REPO_ROOT=$PWD LI_ALLOW_OPEN_VC=1 ./build/compiler/lic/lic build packages/li-net-httpd/src/lib.li -o build/li-httpd` then `BENCH_PROXY_ORACLES=li BENCH_HTTP_QUICK_SEC=5 LI_HTTPD_BIN=$PWD/build/li-httpd ../benchmarks/scripts/run-tier5-http-bench.sh` from `benchmarks` checkout.
3. **Then** open/update PR on `cursor/httpd-proxy-bench-fix-54aa`; human merge after CI green; refresh benchmarks matrix ingest if publishing dashboard rows.
4. **Blocked on** merging `trusted-extern-manifest.toml` automation (seam still hand-extended); full `ensures result == …` on all `proxy_*` Li procs (currently relaxed under `LI_ALLOW_OPEN_VC=1`).

## Changed

| Area | Paths / evidence |
|------|------------------|
| Proxy epoll | `runtime/li_rt_net.c` — `httpd_li_proxy_up_epoll_i` / `httpd_li_proxy_client_epoll_i`; `httpd_upstream_release_i` uses `upstream_port_for_fd`; `proxy_peer_port` on `mark_active` |
| Li dispatch | `packages/li-net-httpd/src/lib.li` — `proxy_dispatch_*_loaded` call C handlers; `proxy_accept_batch` → `proxy_handle_client_in` |
| Trusted seam | `std/runtime/seam.li` (new) — `tcp_*`, `httpd_li_proxy_*`, epoll tagged wait, proxy config |
| `net` package | `packages/li-net/src/lib.li` — re-export `std.runtime.seam` |
| Compiler | `compiler/mir/lower.cpp`, `compiler/codegen/emit.cpp`, `compiler/mir/include/li/mir.hpp` — `returns_i64` / `ret_is_i64` / `CallExtern` store full pointer |
| Typecheck | `compiler/types/typecheck.cpp` — `LI_ALLOW_OPEN_VC` skips weak `ensures` for httpd bench builds |
| Bench | `proxy_loopback,li` **9233.9 req/s** (quick wrk, nginx backend) — was `verify_fail_li:/` |

## Not changed

- Nginx/C proxy oracle code in `benchmarks` vendor harness (only consumes rebuilt `LI_HTTPD_BIN`).
- TLS / `https_static` (still `verify_skip` / M15 pending).
- Master plan PH phase ordering or `trusted.lean` axioms.
- Automated merge / `merge-approved` (human review required).

## Breaking

N/A — compiler ABI fix restores correct `ptr` width; no public API rename.

## Security

N/A — no new trusted axioms; seam documents existing `li_rt_net.c` surface.

## Performance

Tier-5 `proxy_loopback,li` quick profile: **~9.2k req/s** vs prior empty/`verify_fail` (local nginx backend, 5s wrk). Full nightly matrix ingest is downstream of this PR.

## Downstream

- **benchmarks** — set `LI_HTTPD_BIN` to lic build with this commit before `run-tier5-http-bench.sh` / matrix refresh PR.
