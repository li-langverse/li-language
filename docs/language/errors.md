# Li compiler error catalog

**Audience:** Li users and contributors adding diagnostics in `lic`.  
**Source of truth:** `compiler/diagnostics/include/li/error_codes.hpp` — keep this handbook in sync.

Every user-facing error uses a stable **`E####`** code, plain-language **message**, and actionable **hint**.  
Human output: `file:line:col: error [E0301]: …` with a `hint:` line.  
JSON (`lic check --format=json`): `"code":"E0301"`, `"fix_hint":"…"`.

## Categories

| Prefix | Category |
|--------|----------|
| E01xx | Parse / indentation |
| E02xx | Types & indices |
| E03xx | Contracts, borrow, policy |
| E04xx | Control flow |

## Catalog (v1)

| Code | Category | Message (template) | Fix hint |
|------|----------|-------------------|----------|
| **E0101** | parse | Indentation problem near this line. | Use spaces only (no tabs); indent blocks by 2 spaces after `:`. |
| **E0201** | type | This index is outside the array — the program cannot prove it is safe. | Use a constant index, a refinement-typed loop variable, or a `requires` proof. |
| **E0202** | type | Type mismatch (expected …, got …). | Adjust types or add an explicit conversion with proof. |
| **E0301** | contract | Every proc must state what must be true before it runs (`requires`). | Add `requires <condition>` above `=` (`requires true` while developing). |
| **E0302** | contract | Every proc must state what it guarantees on exit (`ensures`). | Add `ensures <condition>` (`ensures true` temporarily if needed). |
| **E0310** | borrow | Borrow conflict (mutable/immutable overlap). | End active `borrow` / `borrow mut` bindings before reusing the value. |
| **E0311** | borrow | Variable was moved and cannot be used again. | Use the new owner or borrow before the move. |
| **E0320** | policy | `parallel for` needs a proved disjointness obligation. | Add `requires disjoint_elem(...)` or `@parallel(disjoint=…)`. |
| **E0321** | policy | `@parallel` missing `disjoint=` proof argument. | `@parallel(disjoint=disjoint_elem(…))`. |
| **E0330** | policy | Name is reserved for the standard library. | Rename, or define under `std/` when extending the prelude. |
| **E0340** | policy | The type `Any` is not allowed. | Use a concrete type or generic `T`. |
| **E0350** | policy | Parallel loop may write shared memory from multiple threads. | Prove disjoint access or use per-iteration private buffers. |
| **E0401** | control | `break` only inside `while` / `for`. | Move into a loop body or remove. |
| **E0402** | control | `continue` only inside `while` / `for`. | Move into a loop body or remove. |

## Adding a new code

1. Add enum value + string in `error_codes.hpp` / `error_codes.cpp`.
2. Add a row to this table.
3. Call `diag_error(…, ErrorCode::…, message, hint)` from the compiler.
4. Add a `compile_fail` or smoke fixture under `li-tests/errors/`.

## Related

- [Errors & control flow spec](../superpowers/specs/2026-05-16-li-errors-and-control-flow.md)
- [Strict by default](../ecosystem/strict-by-default.md)
- [Provability gaps — G-errors](../verification/provability-gaps.md)
