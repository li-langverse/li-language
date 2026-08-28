# Strict by default

**Status:** Binding policy for `lic`, agents, and official packages.  
**Applies to:** every `lic build`, `lic check`, CI job, and agent session in this workspace.

## Principle

**Strict by default.** Mathematical proof, security, and performance gates are **always on at maximum** for every build and check. There is **no optional provability** — users may **only relax** behavior by **explicitly editing** project defaults (for example `li.toml` `[gates]` or documented env flags labeled as downgrades). Silence or omission never weakens a gate.

## Three pillars (always on)

| Pillar | Default gate | What “maximum” means |
|--------|--------------|----------------------|
| **Proof** | Lean 4 + contracts | Every `proc` has `requires` / `ensures`; loops have `invariant` + `decreases`; `lic build` is the proof certificate path; no `sorry`, bare `cast`, or unproved `parallel for` in user code |
| **Security** | stdlib seal, CVE suites, policy | `check_stdlib_seal`, exploit harnesses, trusted surface capped in `docs/semantics/trusted.lean` — agents do not weaken seal or skip CVE rows for convenience |
| **Performance** | Benchmark tiers, honest columns | Regressions investigated; perf claims require bench evidence; dashboard columns stay honest (no “fast” without data) |

Pillar priority when they conflict: **proof → syntax → speed** (see [language design spec](../superpowers/specs/2026-05-14-li-language-design.md)).

Implementation maturity is separate from policy: gaps in [Provability gaps](../verification/provability-gaps.md) describe **compiler wiring**, not a user toggle to turn proof off.

## What “lowering” means

Lowering is **never implicit**. It requires an **explicit, reviewable** project or environment choice:

| Mechanism | Example | Notes |
|-----------|---------|--------|
| **`li.toml` `[gates]`** | `proof.lean = "off"` | Must be present in manifest; CI may reject on protected branches |
| **Documented CLI downgrade** | `--no-lean-verify`, `--allow-open-vc` | Local dev / manifest `verify_open_ok` only; compiler warns on skip |
| **Human-approved CI exception** | Tracked PH-/issue | Agents escalate; do not land silently |

Forbidden as ways to lower gates:

- Omitting `requires` / `ensures` “for now”
- Default-off Lean verify without a named downgrade
- `--no-verify`, hook bypass, or skipping `li-tests` / security harness unless the user **explicitly** requests a documented downgrade path
- Agent suggestions to use `sorry`, `unsafe`, or weaken stdlib seal “to unblock” without user-requested downgrade

## Agent rules

1. Read this page at session start with [engineering standards](engineering-standards.md) and the master plan.
2. **Do not** suggest `--no-verify`, `git commit --no-verify`, or skipping contract/Lean/CVE/bench gates unless the user **explicitly** asks for a documented downgrade.
3. New `proc` / public API: add contracts in the same change.
4. Before claiming done: run the gates relevant to the diff (`./li-tests/run_all.sh`, `scripts/ci.sh`, stdlib coverage when touching `std/`).
5. If a gap is **implementation** (not policy), update [provability-gaps.md](../verification/provability-gaps.md) — do not add a user-facing “proof optional” flag without RFC.

## Related

- [Engineering standards](engineering-standards.md) — functionality, security, performance
- [Provability gaps](../verification/provability-gaps.md) — honest compiler maturity
- [`li.toml` gates](../language/li-toml.md) — explicit downgrade schema (stub)
- `.cursor/rules/li-strict-by-default.mdc` — agent enforcement
- `.cursor/skills/strict-by-default-gate/SKILL.md` — per-feature gate checklist
