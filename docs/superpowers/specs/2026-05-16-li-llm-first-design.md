# Li LLM-first design (research stub)

**Date:** 2026-05-16  
**Status:** Planning / research  
**Pillar priority:** Provability (#1) unchanged — this spec optimizes **agent ergonomics**, not proof shortcuts.

## What this page is for

Capture how Li language, tooling, and docs should minimize **token cost** for LLM agents reading and editing code, without weakening `lic build` or Lean contracts.

**Prerequisites:** [2026-05-14-li-language-design.md](2026-05-14-li-language-design.md), [agent-handover-formats.md](../../ecosystem/agent-handover-formats.md).

## Principles

1. **Agents read more than humans** — prefer stable, grep-friendly symbols and compact manifests over prose duplication.
2. **Structured beats pretty** — machine JSON for diagnostics, edits, and symbol indexes; human ANSI remains default on the terminal.
3. **Infer locally, prove globally** — IDE/`lic check` may omit redundant contract boilerplate in *display* layers only; **`lic build` still requires discharged proofs**.
4. **One canonical entry** — `docs/ecosystem/li-agent-manifest.toml` lists commands agents should call; avoid scattering ad-hoc scripts in prompts.
5. **Diff-friendly** — small files, stable ordering, avoid generated noise in primary source trees.

## Non-goals

| Non-goal | Reason |
|----------|--------|
| Skip `requires` / `ensures` / `decreases` in shipped source | Violates pillar 1 |
| `sorry`, `Any`, or trust-by-prompt | Forbidden in user code |
| Replace Lean with LLM “verification” | Proof certificate stays kernel-checked |
| Break default `lic check` human output | JSON is opt-in (`--format=json`, `lic diagnose`) |
| Terse syntax that cannot elaborate to Core | Sugar must desugar to provable core |

## Concrete ideas (phased)

### Syntax & surface (research)

- **Optional terse aliases** for common contract patterns (desugar to full `proc` specs).
- **Symbol compression** in internal IR / edit buffers (agent applies patches to compact JSON, human view expands).
- **Structured `import` manifest** — single `symbols.li.json` per package for cross-file navigation without parsing all sources.

### Tooling (v0 shipped in this repo)

| Idea | Status |
|------|--------|
| `lic check --format=json` | **Implemented** — `docs/schemas/diagnostic-v1.json` |
| `lic diagnose` | **Implemented** — JSON to stdout |
| `scripts/lic-fix-suggest.sh` | **Stub** — jq hints from JSON |
| Compact test manifest slice for agents | **Implemented** — `scripts/export-li-tests-agent-slice.sh` → `li-tests/agent-manifest.json` |
| `lic edit --patch=json` | **Spec only** — compact edit IR |

### Docs & rules

- `.cursor/rules/li-llm-first.mdc` — token cost checklist for new syntax/docs.
- Agent skill: `.cursor/skills/agent-diagnose-fix-li/SKILL.md`.

## Learned from (survey sketch)

| System | Takeaway for Li |
|--------|-----------------|
| LSP | Stable locations + codes; we mirror in JSON diagnostics |
| MCP tool descriptors | JSON Schema shapes for agent tools → our diagnostic schema |
| OpenAI function calling | Strict JSON schema for machine steps |
| AGENTS.md / Cursor rules | Repo-level entry; Li uses manifest + generated fragment |
| A2A / Devin handoffs | Task envelopes with command + evidence; Li: manifest + JSON diag + test script |

## Conflict resolution

When LLM-first convenience conflicts with provability: **provability wins** (same as language design spec).

## Related

- [Agent handover formats](../../ecosystem/agent-handover-formats.md)
- [li-agent-manifest.toml](../../ecosystem/li-agent-manifest.toml)
- [Provability gaps](../../verification/provability-gaps.md)
