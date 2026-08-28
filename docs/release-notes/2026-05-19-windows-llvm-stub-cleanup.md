# Windows LLVM discovery + numerics stub cleanup

## Summary

Windows CI skips the lic build when Chocolatey LLVM lacks `LLVMConfig.cmake` (ubuntu+macos remain the gate); `li-math-numerics` drops duplicated `extern proc` contract lines.

## Agent continuation

1. Read `.github/workflows/ci.yml` Windows job `Resolve LLVM_DIR` step.
2. Run `./scripts/check-li-def-syntax.sh` and package workspace build on Ubuntu CI (no local LLVM required).
3. Next: re-sync package mirrors if `packages/li-math-numerics` gains a published repo.
4. Blocked: none.

## Changed

- `.github/workflows/ci.yml` — `PATH`, `find` fallback; set `SKIP_WINDOWS_LIC_BUILD` instead of failing the job.
- `packages/li-math-numerics/src/lib.li` — single `requires`/`ensures`/`decreases` per `extern proc`.

## Not changed

- `def` enforcement policy (landed in #58).
- Official mirror repos (already synced in #61 follow-up PRs).

## Breaking

N/A — CI and stub hygiene only.

## Security

N/A — no trust boundary change.

## Performance

N/A.

## Downstream

Package mirror CI unchanged; numerics is monorepo-only today.
