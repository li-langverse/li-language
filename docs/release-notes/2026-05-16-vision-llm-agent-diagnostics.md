# Release notes: 2026-05-16 — vision-llm-agent-diagnostics

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** Vision-LLM (partial), master plan tracker  
**Author:** agent

---

## Summary (one sentence)

Adds opt-in JSON diagnostics (`lic check --format=json`, `lic diagnose`), agent handover docs/manifest stubs, and LLM-first research scaffolding without changing human-default `lic check` output or proof gates.

## Agent continuation (required)

1. Read: `docs/superpowers/specs/2026-05-16-li-llm-first-design.md`, `docs/ecosystem/agent-handover-formats.md`, `docs/schemas/diagnostic-v1.json`
2. Run: `./scripts/build.sh && ./li-tests/tooling/diagnose_json_smoke.sh && ./li-tests/run_all.sh`
3. Then: populate `fix_hint` objects; export compact symbol manifest from `li-tests/manifest.toml`; wire `gen-li-agent-manifest.sh` in CI if desired
4. Blocked on: **none** for scaffolding; full terse-syntax RFC needs human approval (pillar 1)

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| diagnostics | `print_diagnostics_json`, inferred `code` | `compiler/diagnostics/diagnostics.cpp` |
| CLI | `--format=json`, `lic diagnose` | `compiler/lic/main.cpp` |
| schema | `diagnostic-v1` | `docs/schemas/diagnostic-v1.json` |
| docs | LLM-first spec, handover comparison | `docs/superpowers/specs/2026-05-16-li-llm-first-design.md`, `docs/ecosystem/agent-handover-formats.md` |
| agent entry | manifest TOML + gen script stub | `docs/ecosystem/li-agent-manifest.toml`, `scripts/gen-li-agent-manifest.sh` |
| CI | smoke script | `li-tests/tooling/diagnose_json_smoke.sh`, `scripts/ci.sh` |
| tracker | Vision-LLM partial row | `docs/superpowers/plans/2026-05-14-li-master-plan.md` |

## Not changed (scope fence)

- Lean / `lic build` proof pipeline — **not** modified
- Terse syntax / compact edit IR — **spec only**
- `fix_hint` structured patches — **null stub**
- Default human `lic check` stderr formatting — **unchanged**

## Breaking changes

None.

## Security

N/A — diagnostics JSON does not expose secrets; no new trusted axioms.

## Performance

N/A — JSON path is opt-in IDE/agent use; no bench regression claimed.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A — optional copy of manifest pattern later |

## CHANGELOG entry (paste into Unreleased)

See `CHANGELOG.md` [Unreleased] Added section.
