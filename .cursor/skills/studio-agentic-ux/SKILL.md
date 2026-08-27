---
name: studio-agentic-ux
description: >-
  Agentic product UI patterns for Li Studio — task status, tool progress,
  cancel, errors, command palette. Use when building agent chrome or reviewing
  AI-facing surfaces.
---

# Studio agentic UX

Combines Li SOTA (`ux-harness/sota/manifest.yaml` → `agentic_ai`) with community skills ([ui-skills](https://github.com/ibelick/ui-skills), [ui-ux-pro-max](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill)).

## Required patterns (agent-facing UI)

| Pattern | Requirement |
|---------|-------------|
| **Task state** | Visible: idle / running / blocked / failed / done |
| **Progress** | Step list or stream; no silent long runs |
| **Cancel** | Obvious, one click; confirm only if destructive |
| **Errors** | Actionable message + retry; no raw stack in main chrome |
| **Context** | What the agent is editing (file, scene, selection) |
| **Undo** | Last agent action reversible when safe |

## SOTA research (≥3 URLs per assessment)

Read `agentic_ai` entries in `ux-harness/sota/manifest.yaml`, then compare:

- Cursor agent overview (status, tools, diff review)
- Linear (palette, keyboard, fast panels)
- Copilot / Codex (inline suggestions, cancellation)

## Density & composition ([ui-skills](https://github.com/ibelick/ui-skills))

- Prefer **fewer, stronger** panels over dashboard sprawl
- Primary action per region; secondary in overflow/menu
- Stable layout — avoid shifting chrome during agent stream

## Studio-specific (UX-06)

Score agent chrome in the rubric; document gaps when only HTML mock exists.

## Do not

- Hide agent failure behind spinners
- Auto-run destructive scene edits without confirmation
