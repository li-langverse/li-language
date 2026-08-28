# Studio / game-dev docs: `def` not bare `proc`

## Summary

World Studio and agent-facing docs now teach **`def`** only; bare **`proc`** is documented as parser legacy, with **`extern proc`** reserved for FFI.

## Agent continuation

1. Read `.cursor/rules/li-def-not-proc.mdc`, `li-world-studio-vision.mdc`, and `docs/game-dev/world-studio-vision.md` before editing game-dev or studio packages.
2. Run `rg '\bproc\b' docs/game-dev packages/li-studio packages/li-ui packages/li-scene .cursor/skills/create-li-package .cursor/rules/li-world` — hits should be disclaimers, `extern proc`, or "not proc" only.
3. Next: sync `li-def-not-proc.mdc` to `studio` / `ui` org repos when opening mirror PRs; keep package `src/**/*.li` on `def`.
4. Blocked: none for docs-only slice.

## Changed

- `docs/game-dev/world-studio-vision.md` — compiler row clarifies docs/agents use **`def`** only.
- `docs/game-dev/specs/*-rfc.md` — canonical **Li syntax** block on all RFC stubs.
- `.cursor/rules/li-world-studio-vision.mdc`, `li-composable.mdc`, `li-strict-by-default.mdc`, `li-easy-imports.mdc`.
- `.cursor/skills/create-li-package`, `composable-li-library`, `strict-by-default-gate`; `.cursor/hooks/composable-package-nudge.sh` (`^def `).
- `packages/li-log/PUBLISH.md` — API doc wording.

## Not changed

- Compiler parser, `compile_fail` proc rejection tests, or `extern proc` FFI semantics.
- Master-plan / httpd superpowers plans outside `docs/game-dev/**`.
- `std/**` and unrelated `packages/*` source (except `li-log/PUBLISH.md`).

## Breaking

N/A — documentation and agent policy only.

## Security

N/A — no runtime or trust-boundary change.

## Performance

N/A — no bench or hot-path change.

## Downstream

- **studio** / **ui** repos: copy `.cursor/rules/li-def-not-proc.mdc`; README syntax line; CHANGELOG pointer.
- Package mirrors already run `check-li-def-syntax.sh` in CI.
