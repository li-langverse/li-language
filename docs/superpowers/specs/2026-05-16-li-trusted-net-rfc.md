# RFC: Trusted `Net` surface for li-httpd (P0)

**Status:** accepted (2026-05-22) — ships with `w0-bytes-io` on `lic`.  
**Phase:** H / httpd P0 (`w0-bytes-io`, `w1-async-reactor`).

## Goal

Socket I/O lives outside user proofs. User handlers prove against **abstract** `Net`/`Async` in Lean; implementations are **trusted** + audited like `trusted.lean` SDL axioms.

## Allowed axioms (v1)

| Axiom family | Operations | Bounds |
|--------------|------------|--------|
| `TcpListen` | bind, listen, accept | fd cap per worker; backlog ceiling |
| `TcpConn` | send, recv, shutdown, close | bytes ≤ buffer capacity; no unbounded alloc per read |
| `Udp` | sendto, recvfrom | datagram size cap |
| `Dns` | resolve (optional M2) | timeout + result size cap |

**Forbidden in v1 trusted block:** `exec`, arbitrary `open`, user-supplied syscall numbers, `ioctl` escape hatches without RFC.

## Effects

```text
raises Net    — syscall surface above
raises Async  — await reactor readiness (spec: execution-decorators / effects)
```

`lic build` must reject handlers that call trusted ops without `raises Net` in signature.

## Proof obligations (user code)

- Parser buffers: `requires 0 <= off <= buf.len`
- Connection state machine: `decreases` on phase depth
- Timeouts: task eventually completes or closes (no infinite await without timer)

## Implementation seam

- Li surface: `std/runtime/seam.li` (`extern proc tcp_* raises Net`)
- C implementation: `runtime/li_rt_net.c` (single audited syscall shim)
- Package re-export: `packages/li-net`

## Learned from

| Source | Keep | Reject |
|--------|------|--------|
| nginx event module | epoll/kqueue one loop per worker | giant `if` chains in user config |
| Rust Tokio | structured tasks + cancellation | unproved runtime magic |
| Go net | clear conn lifecycle | goroutine without bounds proof |

## Exit gate

- [x] Row in `docs/semantics/trusted.lean` (minimal v1 TcpListen/TcpConn)
- [x] `li-tests/net_trusted/` compile policy tests
- [x] Provability gaps **G-net** row documents partial codegen + effect propagation
- [x] li-httpd plan `w0-bytes-io` ships `std/bytes` Reader/Writer + `raises Net`
