# HTTP tier5 nginx bench parity (m1-nginx-bench-parity)

## Summary

Wires **li-httpd** into the tier5_http bench harness for agent-gateway scenarios (`static_small`, `keepalive_pipelining`), adds a CI gate that verifies correctness on live `build/li-httpd` and optional wrk timing ratios vs nginx, and pre-caches `GET /file.bin` in the runtime for bench hot paths.

## Agent continuation

1. **Gate:** `./scripts/check-tier5-nginx-bench-parity.sh` (verify always; timing when `wrk` + nginx present).
2. **Full timing:** `PATH=/usr/sbin:$PATH wrk` + `HTTPD_BENCH_DURATION_SEC=30 ./scripts/check-tier5-nginx-bench-parity.sh` with `HTTPD_BENCH_SKIP_TIMING=0`.
3. **Pages:** refresh benchmarks matrix when new CSV rows land (`../benchmarks` + `refresh-live-sites.sh`).

## Changed

- `runtime/li_rt_net.c` — prebuilt `/file.bin` response cache at `httpd_prepare_root`.
- `benchmarks/harness/http_bench_servers.py`, `bench_http_parity.py` — shared server start + ratio checks.
- `benchmarks/harness/bench_http.py`, `verify_http.py` — nginx + li timing/verify.
- `benchmarks/tier5_http/suite.toml` — `[profiles.parity]`.
- `scripts/check-tier5-nginx-bench-parity.sh`, `scripts/httpd-plan-gates.sh`.

## Performance

Agent-gateway static workloads: li-httpd targets **≥0.95× nginx RPS** and **≤2× nginx p99** on parity profile (configurable via `HTTPD_BENCH_RPS_RATIO_MIN` / `HTTPD_BENCH_P99_RATIO_MAX`).
