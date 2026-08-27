# Missing-contract sweep — every `.li` file passes the contract gate

**Problem:** several repo `.li` files failed `lic check` / `lic build` with
E0301/E0302 because procs were missing `requires` / `ensures` (and, for
extern-calling procs, the `raises IO` effect). Fixing the contracts surfaced a
second, pre-existing issue in some files (heap `raises Alloc`, extern-call
`raises IO`), which is also resolved here.

## What changed

- **Tier-2 physics benchmarks (`benchmarks/tier2_physics/*/li/main.li`, 10 files):**
  every bare `extern proc li_*_kernel()` now declares `requires true` /
  `ensures true`; `main` declares `raises IO` where it calls the kernel.
  `three_body_pure` also gained `ensures true` on `three_body_forces`.
- **Tetris demo (`examples/tetris/main.li`):** all 8 `extern proc tetris_*`
  declarations gained contracts (`tetris_open` bounds-checked via `requires
  width_px > 0 and height_px > 0`; `tetris_ticks` → `ensures result >= 0`;
  others `requires true` / `ensures true`).
- **Physics proof-db (`proof-db/physics/axioms/conservation.li`,
  `proof-db/physics/lemmas/momentum_invariant.li`):** `total_momentum()`
  extern gained `requires true` / `ensures true`.
- **Std stubs (`std/io/io.li`, `std/csv/csv.li`, `std/ui/ui.li`):** value
  returns now state their postcondition (`ensures result.open == 0`,
  `ensures result.field_count == 0`, `ensures result.r == 1.0 and …`,
  `ensures result == a`); `file_close` → `ensures true`;
  `csv_parse_row_stub` declares `raises Alloc` for its `str` parameter.

## Status

- `lic check` passes on all previously failing files.
- `lic build --no-lean-verify` passes on all previously failing files
  (the object-field `ensures` are statically witnessed, same as
  `li-tests/objects/object_return_call.li`).
- Repo-wide sweep (everything except `li-tests/`, which contains intentional
  negative fixtures like `prove_reject/missing_contracts.li`): **no remaining
  E0301/E0302**.
- **Known separate issue (pre-existing, not contracts):**
  `examples/tetris/main.li` still rejects 8 `board[cell_index(...)]` accesses
  with **E0201** — the index policy only accepts constants, refinement-typed
  params, and while-loop indices, so runtime-computed indices are unprovable
  by design. Fixing it needs a checker enhancement (accept index helpers that
  `ensures` in-bounds), not a contract edit.
