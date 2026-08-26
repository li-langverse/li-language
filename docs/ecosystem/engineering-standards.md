# Engineering Standards

<!-- DOC-ecosystem-stub-engineering-standards.md -->

**Canonical doc:** [engineering-standards.md](https://github.com/li-langverse/roadmap/blob/main/docs/ecosystem/engineering-standards.md) in [`li-langverse/roadmap`](https://github.com/li-langverse/roadmap).

Do not edit ecosystem policy here — open a PR to the roadmap repo (human merge for governance paths).

## Strict by default (lic — binding here)

**Canonical in this workspace:** [strict-by-default.md](strict-by-default.md)

Proof, security, and performance gates are **always on at maximum** for every build/check. There is **no optional provability** — users may only relax via explicit `li.toml` `[gates]` or documented CLI flags on `lic build` (`--no-lean-verify`, `--allow-open-vc`). Silence or omission never weakens a gate.

The three engineering gates below remain **strict**; strict-by-default is how they apply by default.

## Composability gate (lic — binding here)

**Canonical in this workspace:** [composable-by-default.md](composable-by-default.md)

New or materially changed **packages**, **`std/**` slices**, and long-lived tools (httpd, bench runners, `lip`/`lit` surfaces) must:

| Check | Evidence |
|-------|----------|
| **Library API** | `src/lib.li` exports lifecycle verbs (start/stop/ready or equivalent) |
| **Thin entry** | `src/main.li` is demo-only when present — not the sole surface |
| **Import test** | `li-tests/composable/` or package `li-tests/` imports `lib`, not exec-only binary smoke |
| **Contracts** | Same strict-by-default rules on every exported `proc` |

## Related

- [Roadmap milestones](https://github.com/li-langverse/roadmap/blob/main/docs/roadmap/milestones.md)
- [Benchmarks dashboard](https://li-langverse.github.io/benchmarks/)
- [Strict by default](strict-by-default.md)
- [lic master plan](https://github.com/li-langverse/lic/blob/main/docs/superpowers/plans/2026-05-14-li-master-plan.md)
