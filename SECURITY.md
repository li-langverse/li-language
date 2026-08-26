# Security policy

## Reporting a vulnerability

If you believe you have found a security issue in the **Li compiler (`lic`)**, the language toolchain, or official `li-langverse` packages:

1. **Do not** open a public GitHub issue with exploit details before coordinated disclosure.
2. Email the maintainers (see repository contacts) with:
   - Affected commit or release
   - Minimal reproduction (`.li` file, command line, platform)
   - Impact assessment (crash, memory corruption, proof bypass, etc.)
3. We aim to acknowledge within **5 business days** and provide a fix timeline when confirmed.

Parser crashes on malformed input and parallel-memory policy bypasses are in scope. Issues in third-party C libraries (LLVM, OpenSSL, zlib) should also be reported upstream; we track them in [`security/cve-catalog.json`](security/cve-catalog.json).

## CVE-informed testing

Li maintains a **pinned CVE catalog** mapped to tests:

| Artifact | Purpose |
|----------|---------|
| [`security/cve-catalog.json`](security/cve-catalog.json) | Curated C/C++ memory-safety CVE classes → Li mitigations |
| [`security/cwe-to-li-tests.toml`](security/cwe-to-li-tests.toml) | CWE → test obligation |
| [`li-tests/cve_patterns/`](li-tests/cve_patterns/) | Language-level rejections (must not compile) |
| [`scripts/check-cve-coverage.sh`](scripts/check-cve-coverage.sh) | PR gate: catalog ↔ tests stay aligned |

**Refresh policy:**

- **Pull requests** use the pinned catalog only (no network).
- **Weekly** workflow runs `scripts/fetch-cve-catalog.py` against [NVD](https://nvd.nist.gov/) and [OSV](https://osv.dev/) and opens a bot PR when entries change.
- Optional `NVD_API_KEY` repository secret improves rate limits.

**Review model:** ingest and catalog diff are **fully automated**. Bot PRs are merged when `check-cve-coverage.sh` passes on **Linux, macOS, and Windows**; a human reviews only when mappings change (`li_test` paths, new CWE rows, or disputed severity).

**Operating systems:** every CWE harness in `scripts/ci-security.sh` runs on all three CI platforms (`.github/workflows/ci.yml`). Linux-only jobs (libFuzzer, ASan, UBSan) supplement but do not replace the cross-platform gates.

## Historic bug classes (language design for a future OS)

[`security/historic-bugs.toml`](security/historic-bugs.toml) records **well-known failures** (Heartbleed, Shellshock, Dirty COW, Ariane 5, Apple goto fail, Therac-25, etc.) and how Li avoids them at the **language** layer — so a Li-based OS is not vulnerable for the same *design* reasons as typical C kernels.

**Firefly:** if you mean the [Project Serenity Firefly](https://github.com/ProjectSerenity/firefly) research OS, the lesson is “do not build the kernel in unchecked C”; Li’s answer is proved user/kernel `.li` plus a minimal audited C runtime. App-level bugs (e.g. Firefly III IDOR) are tracked as **authorization** gaps, not memory-safety escapes.

`scripts/check-historic-bugs.sh` runs on every OS in CI; registry changes are **mostly automated** review.

## Web server vulnerabilities

[`docs/testing/webserver-security.md`](docs/testing/webserver-security.md) and [`security/webserver-bugs.toml`](security/webserver-bugs.toml) map nginx/Apache classes (smuggling, traversal, SSRF, Slowloris, …) to Li rules **already in the compiler** vs **li-httpd** config/parser work.

## What we test

1. **Compiler robustness** — `li-tests/run_security.sh`, libFuzzer (`parse_fuzz`, `check_fuzz`; Linux CI)
2. **Language semantics** — `li-tests/cve_patterns/`, `race_shared_memory/`, `prove_reject/` (all OS)
3. **Historic registry** — `security/historic-bugs.toml` + policy gates (`cast[`, `goto`, `sorry`, …)
4. **Webserver registry** — `security/webserver-bugs.toml` (`check-webserver-bugs.sh`, all OS)
5. **Trusted C** — `runtime/li_rt.c` and benchmark kernels under ASan in Linux `memory-ci`

See [Security audits](docs/testing/security.md) for the full matrix.

## Out of scope

- Arbitrary third-party exploit binaries run against `lic`
- Claiming complete CVE database coverage
- Side-channel or supply-chain attestation (documented separately)
