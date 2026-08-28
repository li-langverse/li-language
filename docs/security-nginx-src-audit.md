# Nginx source audit (read-only oracle)

li-httpd treats **nginx as a benchmark and security oracle**, not code to port. Vendored nginx lives under `benchmarks/tier5_http/third_party/nginx` (git submodule, pin `release-1.26.2`). The machine-readable checklist is `benchmarks/tier5_http/nginx_mitigations.toml`.

## Run audit

```bash
# Validate checklist schema + src paths (uses submodule when checked out)
./benchmarks/harness/audit_nginx_src.py --check

# Summary: CHANGES digest, CVE count, checklist rows
./benchmarks/harness/audit_nginx_src.py --report

# CI gate (fixture CHANGES + mitigations; no nginx binary)
./scripts/check-tier5-nginx-src-audit.sh
```

CHANGES text: official release `nginx-1.26.2` tarball copied to `benchmarks/tier5_http/fixtures/nginx-1.26.2-CHANGES.txt` because the GitHub mirror tree may not ship `CHANGES` at the repo root.

## Checklist fields

| Field | Meaning |
|-------|---------|
| `id` | Stable mitigation id (referenced from exploit TOML `[nginx_src].mitigation_id`) |
| `cve` | Advisory ids (optional; not all appear in CHANGES) |
| `src` / `symbol` | Read-only nginx path hint for human review |
| `notes` | What nginx mitigates |
| `exploit` | Linked `exploits/*.toml` when present |
| `li_invariant` | Obligation li-httpd should enforce (proofs + runtime) |

**No `li_done`:** this file is a read-only audit checklist, not an implementation tracker.

## Submodule init

```bash
git submodule update --init benchmarks/tier5_http/third_party/nginx
cd benchmarks/tier5_http/third_party/nginx && git checkout tags/release-1.26.2
```

## Gates

```bash
./scripts/check-tier5-nginx-src-audit.sh
./scripts/httpd-plan-gates.sh
```

Exploit replay: [security-http-exploits.md](security-http-exploits.md).
