#!/usr/bin/env python3
"""Proof database catalog CLI — list entries and verify-slice."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import tomllib
except ImportError:  # pragma: no cover
    import tomli as tomllib  # type: ignore

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / "docs/verification/proof-database/schema.toml"


def load_schema() -> dict:
    return tomllib.loads(SCHEMA_PATH.read_text(encoding="utf-8"))


def parse_entries(path: Path) -> list[dict]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    raw = data.get("entry")
    if raw is None:
        return []
    if isinstance(raw, dict):
        return [raw]
    return [e for e in raw if isinstance(e, dict)]


def entries_root(schema: dict) -> Path:
    rel = schema.get("schema", {}).get("entries_root", "docs/verification/proof-database/entries")
    return ROOT / rel


def required_for_kind(schema: dict, kind: str | None) -> list[str]:
    types = schema.get("entry_types", {})
    if not kind or not isinstance(types, dict):
        return []
    block = types.get(kind, {})
    if isinstance(block, dict):
        req = block.get("required", [])
        return list(req) if isinstance(req, list) else []
    return []


def allowed_proof_status(schema: dict) -> set[str]:
    block = schema.get("proof_status", {})
    vals = block.get("values", []) if isinstance(block, dict) else []
    return set(vals) if isinstance(vals, list) else set()


def verify_entry(entry: dict, rel_path: Path, schema: dict, errors: list[str]) -> None:
    eid = entry.get("id", "<no-id>")
    kind = entry.get("kind")
    for field in required_for_kind(schema, kind if isinstance(kind, str) else None):
        val = entry.get(field)
        if val is None or (isinstance(val, str) and not val.strip()):
            errors.append(f"{rel_path}::{eid}: missing required field {field!r}")

    status = entry.get("proof_status")
    allowed = allowed_proof_status(schema)
    if status and allowed and status not in allowed:
        errors.append(f"{rel_path}::{eid}: proof_status {status!r} not in {sorted(allowed)}")

    lean = (entry.get("lean_module") or "").strip()
    if lean:
        lean_path = lean.split("#", 1)[0]
        if lean_path and not (ROOT / lean_path).is_file():
            errors.append(f"{rel_path}::{eid}: lean_module not found: {lean_path}")

    specimen = (entry.get("li_specimen") or "").strip()
    if specimen and not (ROOT / specimen).is_file():
        errors.append(f"{rel_path}::{eid}: li_specimen not found: {specimen}")


def cmd_verify_slice(_args: argparse.Namespace) -> None:
    if not SCHEMA_PATH.is_file():
        print(f"FAIL: schema missing at {SCHEMA_PATH.relative_to(ROOT)}", file=sys.stderr)
        sys.exit(1)

    schema = load_schema()
    root = entries_root(schema)
    if not root.is_dir():
        print(f"FAIL: entries_root missing at {root.relative_to(ROOT)}", file=sys.stderr)
        sys.exit(1)

    errors: list[str] = []
    count = 0
    by_field: dict[str, int] = {}

    for path in sorted(root.glob("*.toml")):
        for entry in parse_entries(path):
            count += 1
            verify_entry(entry, path.relative_to(ROOT), schema, errors)
            fld = str(entry.get("field", "unknown"))
            by_field[fld] = by_field.get(fld, 0) + 1

    if errors:
        for msg in errors:
            print(f"FAIL: {msg}")
        print(f"verify-slice: {count} entries, {len(errors)} error(s)")
        sys.exit(1)

    print(f"verify-slice: OK ({count} entries)")
    for fld, n in sorted(by_field.items()):
        print(f"  field {fld}: {n}")


def cmd_list(args: argparse.Namespace) -> None:
    schema = load_schema()
    root = entries_root(schema)
    for path in sorted(root.glob("*.toml")):
        for entry in parse_entries(path):
            if args.field and entry.get("field") != args.field:
                continue
            eid = entry.get("id", "?")
            kind = entry.get("kind", "?")
            status = entry.get("proof_status", "?")
            field = entry.get("field", "")
            print(f"{eid}	{kind}	{status}	{field}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Proof database catalog tools")
    sub = parser.add_subparsers(dest="command", required=True)

    p_list = sub.add_parser("list", help="List catalog entries")
    p_list.add_argument("--field", help="Filter by field column (e.g. math)")
    p_list.set_defaults(func=cmd_list)

    p_verify = sub.add_parser("verify-slice", help="Validate TOML entries against schema")
    p_verify.set_defaults(func=cmd_verify_slice)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
