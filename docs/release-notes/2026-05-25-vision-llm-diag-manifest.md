# Release notes: 2026-05-25 — vision-llm-diag-manifest

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** **Vision-LLM** (master plan tracker partial)

---

## Summary

Tightens agent-facing JSON diagnostics smoke coverage and adds a `suites` index to the exported `li-tests/agent-manifest.json` slice.

## Agent continuation

1. Read: `docs/superpowers/specs/2026-05-16-li-llm-first-design.md`; `docs/schemas/diagnostic-v1.json`.
2. Run: `./scripts/build.sh && ./li-tests/tooling/diagnose_json_smoke.sh && ./li-tests/tooling/agent_manifest_smoke.sh`
3. Then: advisory JSON (`lic check --deny-warnings`) when WP1 lands; `lic edit --patch=json` spec implementation.
4. Blocked on: **none** for this slice.

## Changed

| Area | What | Evidence |
|------|------|----------|
| Vision-LLM | `diagnose_json_smoke.sh` asserts envelope (`version`, `tool`, `command`), location fields, diagnose/check code parity; optional `jq` parse | `li-tests/tooling/diagnose_json_smoke.sh` |
| Vision-LLM | `agent-manifest.json` export adds sorted `suites` `{name,count}` index | `scripts/export-li-tests-agent-slice.sh`, `li-tests/tooling/agent_manifest_smoke.sh` |
| Tracker | Honest partial Vision-LLM row | `docs/superpowers/plans/2026-05-14-li-master-plan.md` |

## Not changed

- `lic check` / `lic diagnose` compiler output shape — smoke-only tightening.
- `lic edit --patch=json` — still spec-only.
- Advisory lint WP1 (`li-tests/advisory/`) — not in this PR.
- Phase **8p** parallel compile implementation beyond existing merged slice (#200).

## Breaking changes

None.

## Security

N/A — agent ergonomics and test manifest export only.

## Performance

N/A.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |

## CHANGELOG entry

```markdown
### Added
- **Vision-LLM (partial):** `agent-manifest.json` `suites` index; stricter `diagnose_json_smoke.sh` — `docs/release-notes/2026-05-25-vision-llm-diag-manifest.md`.
```
