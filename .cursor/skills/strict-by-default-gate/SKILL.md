---
name: strict-by-default-gate
description: >-
  Before shipping a feature, list which strict gates apply (proof, security,
  performance) and how to document an explicit li.toml downgrade only when the
  user requests relaxation. Use when implementing features, reviewing PRs, or
  when the user mentions strict-by-default or optional proof.
---

# Strict-by-default gate checklist

**Policy:** [strict-by-default.md](../../../docs/ecosystem/strict-by-default.md)  
**Rule:** `.cursor/rules/li-strict-by-default.mdc`

There is **no optional provability** by default.

## Per feature (fill before claiming done)

| Gate | Applies? | Evidence |
|------|----------|----------|
| **Proof** | Contracts on new **`def`** / **`extern proc`**; Lean/VC path if proof surface changed | `li-tests` contract suites; update `provability-gaps.md` if maturity gap |
| **Security** | stdlib seal, CVE/exploit row if attack surface | `run_security.sh` / relevant `li-tests/security/` |
| **Performance** | Hot path or perf claim | Bench row or documented **N/A** in PR |

## Explicit downgrade only

If the user **explicitly** requests relaxation:

1. Add or edit `li.toml` `[gates]` (see [li-toml.md](../../../docs/language/li-toml.md)) — never rely on undocumented defaults-off.
2. Prefer env only for **documented** local dev flags (e.g. `LI_BUILD_VERIFY_LEAN=0`) with compiler warning.
3. Note downgrade in PR / release notes; do not suggest `--no-verify` unless user asked for that path.

## Do not

- Ship without contracts “to iterate”
- Add `gates.proof = false` without user request and review
- Weaken CI or hooks to green a branch

## Related

- `build-li-master-plan` — phase gates
- `li-ecosystem-discipline` — cross-repo CVE/bench
- `write-li-release-notes` — document downgrades in **Changed**
