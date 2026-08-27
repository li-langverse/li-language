# Document LLVM / native ABI for codegen

## Summary

Added `docs/compiler/llvm-abi.md` so agents and `extern` authors do not confuse Li types, MIR flags, LLVM IR, and C `runtime/` signatures.

## Agent continuation

1. **Read** `docs/compiler/llvm-abi.md`, `docs/compiler/build-pipeline.md`, `runtime/li_rt.h`.
2. **Run** `./li-tests/run_httpd_config.sh` after any `emit.cpp` / `lower.cpp` ABI change.
3. **Then** if adding real `Bytes` handles, update llvm-abi.md + C runtime in one PR.
4. **Blocked on** reviewer merge of stacked PR #83 (`cursor/refinement-call-check-57b4`).

## Changed

| Path | What |
|------|------|
| `docs/compiler/llvm-abi.md` | New: pipeline, type table, str/bytes = `i8*`, extern checklist, failure modes |
| `docs/compiler/build-pipeline.md` | Link to llvm-abi |
| `mkdocs.yml` | Nav entry LLVM ABI |

## Not changed

- Codegen implementation (documented only; fixes already on branch in prior commits).
- Lean semantics, benchmark ingest, org merge rules.

## Breaking

N/A — documentation only.

## Security

N/A — no runtime behavior change.

## Performance

N/A.

## Downstream

N/A — docs only; merge with PR #83 stack.
