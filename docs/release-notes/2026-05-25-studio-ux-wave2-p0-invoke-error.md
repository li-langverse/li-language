# Release notes: 2026-05-25 — studio-ux-wave2-p0-invoke-error

**Branch:** `feat/studio-ux-wave2-p0` · **Plan:** `studio-ux-11`, `studio-ux-12`

## Summary

Wave 2 P0: agent invoke mocks (task input, Send, MCP tool trace) and interactive error recovery (Retry/Report/Dismiss), plus `li-studio` invoke compose rects.

## Agent continuation

1. Read `deploy/studio-demo/README.md`, `docs/game-dev/studio-mcp-tools.md`.
2. Run `STUDIO_UI_UX_GATES_SKIP_BUILD=1 ./scripts/studio-ui-ux-plan-gates.sh`.
3. Then `studio-ux-19` reel refresh.
4. Blocked on: PH-GD-5 native host — not this PR.

## Not changed

MCP HTTP server; real wgpu surface probe in compose IR.
