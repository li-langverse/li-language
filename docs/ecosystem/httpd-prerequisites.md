# li-httpd compiler prerequisites (P0)

li-httpd **M1 `.li` code** does not start until these **`lic`** gates pass. Infra in **`lis`** can proceed in parallel.

| ID | Work | Repo | Status |
|----|------|------|--------|
| P0-lean | VC + Lean on `lic build` (real discharge) | `lic` | **Gated for httpd (2026-05-22)** — `check-httpd-lean-gate.sh` + closed `http_parse_forward_closed.li`; composite smokes still ≤8 open VC with `--allow-open-vc`; full kernel **G-lean** partial ([proof-corpus-roadmap](../verification/proof-corpus-roadmap.md)) |
| P0-bytes | `std` bytes, stringview, Reader/Writer | `lic` | **Shipped (2026-05-22)** — [`std/bytes/bytes.li`](../../std/bytes/bytes.li), `bytes_append`/`bytes_byte_at`, Reader/Writer incremental API; gate `check-w0-bytes-io.sh` |
| P0-net | `raises Net`, trusted syscall RFC | `lic` | **Shipped (2026-05-22)** — [trusted-net RFC](../superpowers/specs/2026-05-16-li-trusted-net-rfc.md), [`trusted.lean`](../semantics/trusted.lean) v1 axioms, `li-tests/net_trusted/`, [`runtime/li_rt_net.c`](../../runtime/li_rt_net.c) |
| P0-async | async/await + epoll/kqueue | `lic` | **Shipped (w1)** — MIR `AsyncAwait` → `li_async_poll`/`li_async_await_i32` in `li_rt_net.c` (epoll Linux, kqueue Darwin); `check-w1-async-reactor.sh`; tier5 `tcp_echo` scenario |
| P0-http | HTTP/1.1 parser proofs | `lic` | **Partial** — C epoll `httpd_try_drain_once` static/sendfile + proxy; Li `match_route`/config; full header FSM + Li reactor next |

**Coverage:** `std/**` = **100%**; published `li-*` = **≥80%** ([engineering-standards.md](engineering-standards.md)).

**lis:** [implementation-status](https://github.com/li-langverse/lis/blob/main/docs/implementation-status.md).
