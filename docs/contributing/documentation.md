# Documentation style guide

Li docs should read like a clear technical blog post: precise, friendly, and useful on first read. This guide applies to everything under `docs/` and the root `README.md`.

## Audience

Assume the reader:

- Writes code in Python, Rust, Nim, or C++  
- Cares about simulation/HPC correctness and performance  
- Does **not** already know Li’s phase plan or internal module names  

Define terms on first use. Link to the design spec instead of copying ten-page tables.

## Voice and tone

**Do:**

- Use full sentences  
- State the goal of the page in the first paragraph  
- Explain *why* a design choice exists (e.g. LLVM-only, Python 3.14 types)  
- Give copy-pasteable commands and small complete examples  

**Avoid:**

- Bullet-only pages with no connective prose  
- Internal codenames without context  
- “Simply” / “just” when the step is not simple  
- Promising dates without pointing to the master plan phases  
- Claiming **`lic build` runs Lean** or full proof discharge before **Phase 2f** lands  

## Provability and “today vs target”

When documenting proofs, `lic build`, decorators, parallelism, or math notation:

1. Read **[Provability gaps](../verification/provability-gaps.md)** first.  
2. Use **target** vs **today** language (see that page’s summary table).  
3. When a gap closes in code, update the **gap register** in the **same PR** as the implementation.  

Handbook pages should link to the gap doc where the spec promise exceeds the compiler.

## Page template

```markdown
# Title

One sentence: what this page helps you do.

## Background (optional, 1 short paragraph)

## Main content (sections with headings)

## Traceability and official packages

When documenting **standard** or **first-party** packages:

- Assign a `PKG-*` id and list it in [official-packages.md](../ecosystem/official-packages.md) (see [ecosystem governance plan](../superpowers/plans/2026-05-16-li-ecosystem-governance.md)).
- Link tests via `T-*` notes in `li-tests/manifest.toml` where behavior is normative.
- Use **Keep a Changelog** and **SemVer** for release notes; **SPDX** license identifiers in `li.toml`.

Ecosystem pages use HTML comments for doc IDs: `<!-- DOC-ecosystem-lip -->` (optional, for traceability tooling).

## Related links
```

## Code examples

- Shell blocks: full commands, no `...`  
- Li surface syntax: use ` ```nim ` fences (indentation-based)  
- When teaching types, show one valid example and one compile error  

Example:

```nim
type Board = array[20, array[10, Cell]]   # OK
# board[25, 0] = ...                       # error: row index out of range
```

## Linking

- Prefer relative links within `docs/`  
- Point to the [language design spec](../superpowers/specs/2026-05-14-li-language-design.md) for normative rules  
- Point to [phase plans](../superpowers/plans/2026-05-14-li-master-plan.md) for implementation order  

## Diagrams

Use Mermaid or ASCII for pipelines and phase order when it saves a paragraph of confusion.

## Keeping docs honest

Mark pages **Planning** vs **Active** when the compiler lands. If CMake flags or CLI names change, update `getting-started.md` in the same PR as the code.

## Cursor rules

Editor agents load rules from `.cursor/rules/`:

| Rule | Scope |
|------|--------|
| `li-project.mdc` | Always — project context |
| `documentation-style.mdc` | `docs/**`, README |
| `li-language.mdc` | `**/*.li` |
| `compiler-cpp.mdc` | `compiler/**` |
| `benchmarks.mdc` | `benchmarks/**` |
