# HTTPd w1: async reactor + TCP echo (2026-05-22)

## Summary

- **`li_async_poll` / `li_async_await_i32`** now block on **epoll** (Linux) or **kqueue** (Darwin) in `runtime/li_rt_net.c`; sync stubs removed from `li_rt.c`.
- **`li_async_reactor_register_i`**, **`li_async_reactor_selftest_i`**, **`tcp_echo_epoll_once_i`** trusted seam entries in `std/runtime/seam.li`.
- **MIR link:** `AsyncAwait` / `li_async_*` callees pull `li_rt_net.c`.
- **Gates:** `./scripts/check-w1-async-reactor.sh` (async codegen, reactor selftest, Linux TCP echo smoke).
- **Bench:** `benchmarks/tier5_http/scenarios/tcp_echo/bench.toml` (P0 matrix row; `enabled = false`, still in `suite.toml` exclude until tcpkali nightly).

## Verify

```bash
./scripts/build.sh
./scripts/check-w1-async-reactor.sh
./scripts/httpd-plan-gates.sh
```
