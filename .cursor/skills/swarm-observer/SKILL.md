---
name: swarm-observer
description: >-
  Monitor the li-cursor-agents swarm for failures and goal drift; rely on
  programmatic self-healing in the supervisor. Use when agents fail repeatedly,
  handoffs break, or the user wants unattended swarm updates.
disable-model-invocation: true
---

# Swarm observer (Li ecosystem)

The **control plane** lives in sibling repo `../li-cursor-agents` (not this `lic` tree).

## Unattended operation

1. Start `npm run agents:keep` in `li-cursor-agents` with `CURSOR_API_KEY` in `.env`.
2. The **supervisor** runs `src/observer/` each tick: auto-retry, healer dispatch, `swarm_degraded` only when exhausted.
3. The **`swarm_observer`** agent runs when the swarm is degraded — audits prompts/supervisor, not product code.

## When to invoke this skill in Cursor

- Multiple agent runs failed with `status: error`
- Briefing recommends agents that never run
- User wants hands-off updates without babysitting the dashboard

## Checks

```bash
cd ../li-cursor-agents
curl -s http://127.0.0.1:9477/api/swarm/health | head
cat data/control-plane/latest-report.json | jq '.swarm_health'
```

## Human-only blockers

- Missing `CURSOR_API_KEY` (set per `li-cursor-agents/README.md`)
- Governance merges (`roadmap` PRs)
- LLVM/toolchain missing in `lic` (compiler agents cannot self-heal installs)

Do not weaken Lean/proof gates to “fix” red agents.
