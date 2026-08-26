# Web server vulnerabilities vs Li (language today)

This page maps **common nginx/Apache/httpd CVE classes** to what Li **already rejects in user `.li` code** and what **li-httpd** will prove later. Review is **automated** via `scripts/check-webserver-bugs.sh` on Linux, macOS, and Windows; humans adjust the registry when nginx publishes new classes.

**Registry:** [`security/webserver-bugs.toml`](../../security/webserver-bugs.toml)  
**Future runtime oracles:** [`li-httpd plan`](../superpowers/plans/2026-05-16-li-httpd-plan.md) (exploit TOML vs stock nginx)

---

## Summary table

| Class | Example | CWE | Avoided **today** in Li language? | How |
|-------|---------|-----|-----------------------------------|-----|
| Request smuggling | nginx CVE-2019-20372 | CWE-444 | **No** (no HTTP parser) | httpd: one parser, no duplicate CL |
| Path traversal | `..%252f` alias bugs | CWE-22 | **Partial** | Fixed indices; no path type yet |
| SSRF / open proxy | Apache CVE-2023-25690 | CWE-918 | **Partial** | `raises IO` required; no runtime URL |
| Header CRLF injection | Response splitting | CWE-113 | **Partial** | Undeclared I/O fails; literal headers M1 |
| Parser OOB read | Huge headers/body | CWE-125 | **Yes** | `cve_patterns` OOB index tests |
| TLS heartbeat leak | Heartbleed | CWE-125 | **Partial** | User buffers bounded; TLS is trusted C |
| Slowloris / hung read | Slow headers | CWE-400 | **Yes** | `while` without `decreases` rejected |
| Worker race | Shared cache | CWE-362 | **Yes** | `parallel for` + disjoint policy |
| Shell / CGI injection | Shellshock | CWE-78 | **Yes** (build seam) | Path metacharacter reject |
| nginx `if` / rewrite | Smuggling via config | CWE-444 | **Partial** | No `goto`; httpd bans rewrite DSL |
| Dynamic modules | `load_module` | CWE-829 | **Partial** | `Any` forbidden; no modules in M1 |
| Cleartext public TLS | HTTP on :443 | CWE-319 | **No** (config) | httpd: `tls` required off loopback |
| Log injection | Forged access lines | CWE-117 | **No** (httpd) | li-log redaction (planned) |

**Yes** = enforced by current `lic` on `.li` sources in CI. **Partial** = related gate exists; full class needs httpd types/config. **No** = tracked as `httpd_design` until li-httpd ships.

---

## What the language already gives you

These properties help **any** server written in Li (including a future OS network stack), not only li-httpd:

1. **No memory-smuggling via array bugs** — constant or refinement indices; dynamic `buf[i]` without proof fails compile ([`cwe787_dyn_index.li`](../../li-tests/cve_patterns/cwe787_dyn_index.li)).
2. **No silent proof bypass** — `Any`, `sorry`, bare `cast[`, `unsafe`, `goto` rejected ([`prove_reject/`](../../li-tests/prove_reject/), policy).
3. **No unbounded loops without measure** — Slowloris-style “read forever” loops fail without `decreases` ([`missing_decreases.li`](../../li-tests/prove_reject/missing_decreases.li)).
4. **No undeclared side effects** — networking/logging must declare `raises IO` ([`effects/io_fail.li`](../../li-tests/effects/io_fail.li)).
5. **No data races in parallel workers** — shared mutable slots in `parallel for` fail ([`race_shared_memory/`](../../li-tests/race_shared_memory/)).
6. **No shell injection in the toolchain** — `lic build` rejects dangerous `-o` / `LI_EXTRA_C` paths ([`codegen_path_injection.sh`](../../li-tests/security/codegen_path_injection.sh)).
7. **Extern is explicit** — C seams need contracts ([`cwe676_extern_no_contract.li`](../../li-tests/cve_patterns/cwe676_extern_no_contract.li)).

---

## What httpd adds (not in `lic` today)

From the [httpd plan](../superpowers/plans/2026-05-16-li-httpd-plan.md):

| nginx footgun | Li httpd rule |
|---------------|---------------|
| `alias` traversal | `root` + `canonical_root` at validate |
| `proxy_pass` + `$uri` | `UpstreamId` enum only; peers fixed at start |
| `if` / `rewrite` | Static `MatchExpr` route table |
| `include *.conf` | Single config file |
| `load_module` / Lua | Forbidden |
| Duplicate CL / TE.CL | Single parser + `ensures` |
| Optional TLS on public | `tls = true` unless loopback |

Exploit rows (e.g. `request_smuggling_cl_te.toml`) are **integration tests** after M1 — they do not replace language gates above.

---

## CI

```bash
./scripts/check-webserver-bugs.sh   # registry ↔ tests
./scripts/ci-security.sh            # includes webserver check (all OS)
```

---

## Related

- [Security audits](security.md)
- [Historic bug classes](security.md#historic-failures-os-ready)
- [Provability gaps](../verification/provability-gaps.md) — honest limits until Lean/httpd land
