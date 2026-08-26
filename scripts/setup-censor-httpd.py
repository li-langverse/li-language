#!/usr/bin/env python3
"""li-httpd setup-censor — migrations/OpenAPI hints → leak_censor.generated.toml."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from httpd_leak_censor import write_generated_toml
from schema_catalog import load_applied_manifest, merge_catalog, parse_migrations_dir


def main() -> int:
    p = argparse.ArgumentParser(description="Generate leak_censor.generated.toml from DB migrations")
    p.add_argument(
        "--migrations",
        type=Path,
        required=True,
        help="Directory of applied *.sql migrations",
    )
    p.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("."),
        help="Directory for leak_censor.generated.toml (default: cwd)",
    )
    p.add_argument(
        "--migrations-applied",
        type=Path,
        default=None,
        help="Optional migrations_applied.toml — only SQL files listed as applied in prod",
    )
    p.add_argument(
        "--openapi",
        type=Path,
        default=None,
        help="Optional OpenAPI YAML (v1: ignored except path recorded in comment)",
    )
    args = p.parse_args()

    if not args.migrations.is_dir():
        print(f"setup-censor: missing migrations dir {args.migrations}", file=sys.stderr)
        return 1

    applied_only: list[str] | None = None
    if args.migrations_applied is not None:
        if not args.migrations_applied.is_file():
            print(
                f"setup-censor: missing applied manifest {args.migrations_applied}",
                file=sys.stderr,
            )
            return 1
        try:
            applied_only = load_applied_manifest(args.migrations_applied)
        except ValueError as e:
            print(f"setup-censor: {e}", file=sys.stderr)
            return 1
        if not applied_only:
            print("setup-censor: applied manifest is empty", file=sys.stderr)
            return 1

    catalog = merge_catalog(parse_migrations_dir(args.migrations, applied_only))
    if args.openapi and args.openapi.is_file():
        catalog.sources.append(args.openapi.name)

    out = args.output_dir / "leak_censor.generated.toml"
    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_generated_toml(out, catalog.json_paths, catalog.header_deny, catalog.sources)
    print(f"setup-censor: wrote {out} ({len(catalog.json_paths)} json paths)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
