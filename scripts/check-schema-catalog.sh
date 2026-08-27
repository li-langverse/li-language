#!/usr/bin/env bash
# Schema catalog golden — setup-censor migration parser (Python).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/scripts${PYTHONPATH:+:$PYTHONPATH}"

mig="$ROOT/li-tests/schema_catalog/migrations"
applied="$ROOT/li-tests/schema_catalog/migrations_applied.toml"
golden="$ROOT/li-tests/schema_catalog/golden_paths.txt"
tmp="$(mktemp)"
gen_dir="$(mktemp -d)"

python3 "$ROOT/scripts/setup-censor-httpd.py" --migrations "$mig" -o "$gen_dir"
python3 "$ROOT/scripts/setup-censor-httpd.py" \
  --migrations "$mig" \
  --migrations-applied "$applied" \
  -o "$gen_dir"
python3 - <<PY
from pathlib import Path
import tomllib
from schema_catalog import load_applied_manifest, merge_catalog, parse_migrations_dir

mig = Path("$mig")
applied = Path("$applied")
exp = sorted(ln.strip() for ln in Path("$golden").read_text().splitlines() if ln.strip())
applied_only = load_applied_manifest(applied)
cat_applied = merge_catalog(parse_migrations_dir(mig, applied_only))
if sorted(cat_applied.json_paths) != exp:
    raise SystemExit(
        "applied manifest filter mismatch:\n"
        f"  got={sorted(cat_applied.json_paths)}\n  exp={exp}"
    )
cat_all = merge_catalog(parse_migrations_dir(mig))
full_has_billing = any("billing" in p for p in cat_all.json_paths)
if not full_has_billing:
    raise SystemExit("expected 004_billing_secret.sql to add billing paths when unfiltered")
if any("billing" in p for p in cat_applied.json_paths):
    raise SystemExit("applied filter must exclude pending 004_billing_secret.sql")
gen = tomllib.loads((Path("$gen_dir") / "leak_censor.generated.toml").read_text())
paths = gen["generated"]["json_paths"]["paths"]
if sorted(paths) != sorted(exp):
    raise SystemExit("generated.toml paths mismatch")
print("schema_catalog: OK")
PY
rm -rf "$gen_dir" "$tmp"
