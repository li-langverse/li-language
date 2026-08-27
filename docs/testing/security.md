# Security audits

Li treats **compiler robustness** and **parallel safety** as security properties.

## 1. Malformed input (no crashes)

**Goal:** Attacker-controlled `.li` text must not crash `lic`.

**Harness:** `li-tests/run_security.sh`

| Input | Expectation |
|-------|-------------|
| `empty.li` | Clean exit |
| `deep_indent.li` | Clean exit |
| `many_tokens.li` | Clean exit |
| `unclosed_paren.li` | Clean exit |
| `unclosed_string.li` | Clean exit |
| `nested_parens.li` | Clean exit |
| `truncated_proc.li` | Clean exit |
| `long_line_no_newline.li` | Clean exit |
| `deeply_nested_calls.li` | Clean exit |
| `integer_literal_flood.li` | Clean exit |
| `invalid_utf8_strings.li` | Clean exit |
| `pathological_generics.li` | Clean exit |
| Huge comment (generated) | Clean exit |
| `codegen_path_injection.sh` | Rejects `;`, `$()`, `\|` in link paths |

**Rule:** exit code 0 or 1 only — never signal (segfault, abort).

**CI:** every `scripts/ci.sh` on Linux/macOS; Windows job runs the same harness.

## 2. CVE-informed catalog

**Goal:** Map public C memory-safety CVE classes to Li tests and mitigations.

| Artifact | Role |
|----------|------|
| `security/cve-catalog.json` | Pinned catalog (≥30 entries); PR gate |
| `security/cwe-to-li-tests.toml` | CWE → obligation (`compile_reject`, `fuzz_seed`, …) |
| `scripts/fetch-cve-catalog.py` | NVD + OSV ingest (nightly bot PR) |
| `scripts/check-cve-coverage.sh` | Catalog ↔ `li-tests` alignment |

**Harness:** `li-tests/cve_patterns/` — CWE-named programs that must **not compile** (plus `good_*` controls).

**CI:** `scripts/ci-security.sh` runs on **Linux, macOS, and Windows** (via `scripts/ci.sh` and the Windows CI job):

- Malformed-input corpus (`run_security.sh`)
- All `cve_patterns` manifest entries (every mapped CWE)
- Catalog ↔ test alignment (`check-cve-coverage.sh`)
- Path injection rejects (`codegen_path_injection.sh`)

**Workflow:** `.github/workflows/cve-catalog.yml` validates the catalog on all three OSes on every PR; weekly NVD/OSV refresh opens a bot PR (human review only when mappings change).

See also [SECURITY.md](../../SECURITY.md) for disclosure and refresh policy.

### Historic failures (OS-ready)

[`security/historic-bugs.toml`](../../security/historic-bugs.toml) maps **famous incidents** to Li rules so a future Li kernel does not repeat C-era mistakes:

| Incident | Lesson | Li enforcement |
|----------|--------|----------------|
| Heartbleed / Cloudbleed | OOB read on trust | Constant/refinement indices (`cve_patterns`) |
| Shellshock | Env → shell injection | Path metacharacter reject (`codegen_path_injection.sh`) |
| Dirty COW / Therac-25 | Races on shared state | `parallel for` + disjoint proofs (`race_shared_memory`) |
| Ariane 5 | Silent narrowing / dead path | No bare `cast[…]`; `decreases` required |
| Apple goto fail | Bypass via `goto` | `goto` forbidden in policy |
| Serenity **Firefly** OS | C kernel UB → use safe language | Proved `.li` + tiny audited C seam (design) |

Validated on **Linux, macOS, Windows** via `scripts/check-historic-bugs.sh` (automated; human only when adding rows).

### Web server / reverse-proxy classes

See **[Web server security](webserver-security.md)** for nginx/Apache CVE classes (smuggling, path traversal, SSRF, Slowloris, …) mapped to what Li **already enforces** in the language vs what **li-httpd** will add at config/parse time.

Registry: [`security/webserver-bugs.toml`](../../security/webserver-bugs.toml) — `scripts/check-webserver-bugs.sh` on every OS in `ci-security.sh`.

## 3. Parser fuzzing (continuous)

**Tools:** `parse_fuzz`, `check_fuzz` (libFuzzer + Clang)

**Workflow:** `.github/workflows/fuzz.yml`

| Trigger | Work |
|---------|------|
| Pull request | 5000 runs, parser paths |
| Daily cron | Longer budget, corpus merge |
| Manual | `workflow_dispatch` |

**Corpus learning:**

1. Download prior `fuzz-corpus` artifact.
2. Run fuzz with crash artifacts saved.
3. `scripts/merge_fuzz_corpus.sh` dedupes and adds `regressions/`.
4. Bot opens PR with new seeds (on `main` schedule).

## 4. Parallel race exploits

**Goal:** Unsafe shared memory must **not compile**.

**Harness:** `li-tests/race_shared_memory/` + `manifest.toml`

Documented in [Tests overview](overview.md#race-exploit-suite-security-of-parallelism).

## 5. Policy gate (forbidden constructs)

`compiler/types/policy.cpp` rejects:

- `Any`
- Obvious parallel anti-patterns (shared slot writes, missing disjoint, …)
- **Decorator exploits** (Phase 7d-e): `reserved_name`, `typosquat_reserved`, `decorator_name_too_short`, `reserved_prefix`, `@parallel(` without `disjoint=`

**Harness:** `li-tests/decorator_exploits/` — run via `./li-tests/run_all.sh decorator_exploits` (also in `scripts/ci.sh`).

This complements Lean, not replaces it. See [Provability gaps](../verification/provability-gaps.md) (**G-dec**).

## 6. Memory CI (Linux)

`scripts/memory-ci.sh`:

- Security corpus again
- RSS profiling
- ASan build of MD kernel / `lic` when available
- ASan smoke of `runtime/li_rt.c` (`profile-memory.sh --asan-rt`)

**UBSan:** `.github/workflows/ubsan-lic.yml` (weekly `lic` build with `-DLI_SANITIZE=undefined`).

**Trusted C audit:** `security/trusted-c-audit.toml` — manual matrix for `li_rt.c` and reference kernels.

## 7. What we do not claim yet

| Gap | Status |
|-----|--------|
| MIR / codegen fuzzing | `check_fuzz` covers parse + typecheck only |
| Formal proof of C++ compiler | Roadmap |
| Complete CVE database coverage | Curated catalog + nightly refresh, not exhaustive |
| Side-channel freedom | Out of scope |
| Supply-chain attestation | Standard open-source practice |

## Reporting issues

For security-sensitive reports, follow [SECURITY.md](../../SECURITY.md).

For general robustness bugs, open a GitHub issue with:

- Input file (minimal)
- `lic` version / commit
- Expected vs actual (crash vs diagnostic)
