# Proof-db discrepancy & gap reporter

**Script:** `scripts/proof-db-report.sh` · **Registry:** `proof-db/discrepancies.toml` · **Baseline:** `proof-db/expected.json`

## JSONL fields

`id` (required); `spec`, `lean`, `mir_witness`, `open_goals`, `gap_id`, `failure_mode`, `notes`.

## Policy: failure modes

| Mode | Meaning |
|------|---------|
| `li_bug` | Toolchain bug (VC emit, MIR witness, Lean glue) |
| `wrong_spec` | User contract wrong (**G-wrong-spec**) |
| `open_math` | Intentional open VC |
| `axiomatic_limit` | **G-hw** / trusted limit |

## Discrepancy kinds

`spec_vs_lean` · `lean_vs_mir` · `spec_vs_mir` · `run_delta`
