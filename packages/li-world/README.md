# li-world

Text-line world snapshot save/load for Li Engine (PH-GD-2). No binary format, no filesystem trusted I/O in this scaffold.

Import: `import world`

## Format

`world_v1 name=<id> tick=<n> entity_count=<m>` — single line per snapshot; names are token-safe (no spaces) until full scene graph lands.

## API

- `world_format_version` — format id (`1`)
- `WorldSnapshot` — `name` (stub slot id: 0=`default`, 1=`arena`), `tick`, `entity_count`
- `world_serialize` / `world_parse_line` — line ↔ snapshot
- `world_save_to_buffer` / `world_load_from_buffer` — in-memory string round-trip

## Evidence

- `li-tests/smoke/world_roundtrip.li`
