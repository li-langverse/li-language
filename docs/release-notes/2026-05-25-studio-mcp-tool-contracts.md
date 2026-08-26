# Release notes: 2026-05-25 — studio-mcp-tool-contracts

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-AGENT, AGENT-0  
**Author:** agent

---

## Summary (one sentence)

PH-AGENT scaffold: five Studio MCP tool IDs/names, `StudioAgentToolRequest` on agent chrome, `li_rt` name table, and smoke round-trip — contracts only, no lis MCP server.

## Agent continuation (required)

1. Read: `docs/game-dev/studio-mcp-tools.md`, `docs/game-dev/world-studio-vision.md` §18, `packages/li-studio/src/lib.li` (PH-AGENT block).
2. Run: `lic check packages/li-studio/src/lib.li`; `lic check packages/li-studio/li-tests/smoke/studio_mcp_tools.li`.
3. Then: AGENT-1 — register tools in `lis mcp li-engine` and wire `@cursor/sdk` per `specs/studio-cursor-sdk-rfc.md`.
4. Blocked on: MCP HTTP transport — **none** for contract merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-studio` | `studio_mcp_*` IDs, `studio_mcp_tool_name` / `from_name`, `StudioAgentToolRequest`, `studio_compose_agent_chrome_with_tool`; `li_std_studio_version` → 5 | `li-tests/smoke/studio_mcp_tools.li` |
| `runtime/li_rt.c` | `li_rt_studio_mcp_tool_name`, `li_rt_studio_mcp_tool_from_name` | name round-trip in smoke |
| `compiler/codegen/emit.cpp` | Declare new `li_rt` symbols | link with studio package |
| `docs/game-dev/studio-mcp-tools.md` | Tool table + proof gate note | agent-readable contract |

## Not changed (scope fence)

- `lis` MCP HTTP server — **not** implemented.
- `@cursor/sdk` session wiring — **not** in this PR.
- Extra vision tools (`am_export_print`, `chem_dft_run`, `studio_adaptive_layout`) — **not** in wave-1 ID table.
- LLVM / httpd / tier5 — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** `li_std_studio_version` bumped to `5`; smokes requiring version `4` must update.
- **Security:** N/A — stub IDs only, no network surface.
- **Performance:** N/A — O(1) lookup table.
- **Downstream:** MCP server agents should use documented tool names before AGENT-1 registration.
