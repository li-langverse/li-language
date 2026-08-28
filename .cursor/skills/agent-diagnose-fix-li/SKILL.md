# Agent diagnose & fix (Li)

Use when an agent must fix Li source using machine-readable diagnostics and `li-tests`.

## When to use

- Compile/type errors on `.li` files
- CI red on `lic check` / `lic build`
- User asks for automated fix loop with evidence

## Workflow

1. **Read gates** — `AGENTS.md`, `.cursor/rules/li-ecosystem-gates.mdc` (provability wins).
2. **Diagnose (JSON):**
   ```bash
   lic diagnose path/to/file.li
   # or
   lic check path/to/file.li --format=json
   ```
3. **Optional hints:**
   ```bash
   lic diagnose path/to/file.li | ./scripts/lic-fix-suggest.sh
   ```
4. **Edit** using `file`, `line`, `column`, `code`, `message` from JSON (`docs/schemas/diagnostic-v1.json`).
5. **Verify fast:**
   ```bash
   lic check path/to/file.li
   ```
6. **Verify proof path** (when changing contracts or build-affecting code):
   ```bash
   lic build path/to/file.li -o /dev/null
   ```
7. **Run tests:**
   ```bash
   ./li-tests/run_all.sh <suite>
   # or full: ./li-tests/run_all.sh
   ```
8. **Smoke JSON tooling** (after compiler changes):
   ```bash
   ./li-tests/tooling/diagnose_json_smoke.sh
   ```

## JSON fields

| Field | Use |
|-------|-----|
| `ok` | `false` ⇒ fix before claiming done |
| `code` | Category (`type.index`, `parse.indent`, …) |
| `fix_hint` | Reserved; null in v0 |
| `message` | Human-readable detail |

## Do not

- Treat `lic check` JSON as a proof certificate — use `lic build` for ship gates.
- Weaken contracts because an agent inferred intent.
- Skip `li-tests` after compiler or stdlib changes.

## Canonical entry

`docs/ecosystem/li-agent-manifest.toml`
