# Release notes: 2026-05-25 — vertical-gap-mcp-chem

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/vertical-gap-mcp-chem`  
**PH / REQ:** PH-AGENT (gap #6/#7 partial), PH-QM stub  
**Author:** agent

---

## Summary (one sentence)

Studio MCP contracts extend to eight tool IDs (`am_export_print`, `chem_dft_run`, `studio_adaptive_layout`), `studio_mcp_tool_dispatch` stub, `li-chem` `chem_dft_run_smoke`, and smokes — no `lis` HTTP server.

## Agent continuation (required)

1. Read: `docs/game-dev/studio-mcp-tools.md`, `packages/li-studio/src/lib.li` (PH-AGENT block), `packages/li-chem/src/lib.li`.
2. Run: `lic check packages/li-studio/li-tests/smoke/studio_mcp_extended.li`; `lic check li-tests/composable/import_lig_chem_backend.li`.
3. Then: AGENT-1 — register eight tools in `lis mcp li-engine`; wire `chem_dft_run` to real `li-chem` queue and `am_export_print` to additive export pipeline.
4. Blocked on: MCP HTTP transport and ORCA/Psi4 backends — **none** for this contract merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `docs/game-dev/studio-mcp-tools.md` | Tool table + dispatch API; gap #6/#7 rows | agent-readable contract |
| `runtime/li_rt.c` | Names `am_export_print`, `chem_dft_run`, `studio_adaptive_layout` (IDs 6–8) | `studio_mcp_tool_name` round-trip |
| `packages/li-studio` | `studio_mcp_am_export_print`, `studio_mcp_chem_dft_run`, `studio_mcp_studio_adaptive_layout`, `studio_mcp_tool_count` → 8, `studio_mcp_tool_dispatch` | `li-tests/smoke/studio_mcp_extended.li`, updated `studio_mcp_tools.li` |
| `packages/li-chem` | `chem_dft_run_smoke()`, `chem_dft_energy_stub_hartree()` (−76.0); `chem_lig_backend_auto` unchanged | `li-tests/composable/import_lig_chem_backend.li` |

## Not changed (scope fence)

- `lis` MCP HTTP server — **not** implemented.
- `@cursor/sdk` session wiring — **not** in this PR.
- Real printer export, DFT queue execution, or adaptive panel content — stubs/contracts only.
- `li_std_studio_version` — remains **6** (no bump).
- LLVM / httpd / tier5 / proof-db — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** `studio_mcp_tool_count()` is **8** (was 5); consumers hard-coding count 5 must update.
- **Security:** N/A — stub IDs and dispatch only, no network surface.
- **Performance:** N/A — O(1) name table lookup.
- **Downstream:** MCP server agents should use documented tool names for gap #6/#7 before AGENT-1 registration.
