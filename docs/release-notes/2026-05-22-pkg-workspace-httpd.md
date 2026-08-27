# httpd pkg-workspace — Phase-H package scaffold alignment

## Summary

Aligns the Phase-H httpd package slice with `scripts/li-new-package` and lip § A3: workspace members in `packages/li.toml`, official stubs for planned infra (`li-bytes`, `li-rng`, `li-prob`, `li-crypto`, `li-tls`, `li-acme`), and re-scaffolded `li-schema` / `li-http` trees.

## Changed

| Area | Paths |
|------|--------|
| Workspace | `packages/li.toml` — httpd members + existing numerics |
| Stubs | `packages/li-{bytes,rng,prob,crypto,tls,acme,schema}/` via `li-new-package --official` |
| Re-scaffold | `packages/li-schema/`, `packages/li-http/` (src preserved) |
| Gate | `scripts/check-pkg-workspace.{py,sh}`, `scripts/httpd-plan-gates.sh` |
| Tests | `li-tests/tooling/check_pkg_workspace.sh` |

## Not changed

- `lip install` / registry resolution (8b+).
- Implementation inside stub packages (still smoke-only).
- Live benchmarks Pages (no bench CSV).

## Breaking

N/A — additive workspace members and CI gate.
