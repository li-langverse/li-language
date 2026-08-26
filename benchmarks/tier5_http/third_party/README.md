# third_party (read-only oracles)

| Path | Pin | Use |
|------|-----|-----|
| `nginx/` | `release-1.26.2` (`37fe9835…`) | CVE/bench oracle; **do not** compile into Li |

Init: `git submodule update --init benchmarks/tier5_http/third_party/nginx`

CHANGES for CI gates: `../fixtures/nginx-1.26.2-CHANGES.txt` (from nginx.org tarball).

Audit: `../../harness/audit_nginx_src.py` → `../nginx_mitigations.toml`.
