# Phase 6: Self-host seed

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or build-li-master-plan.

**Goal:** Ship a **bootstrap** `lic` binary compiled from Li source (`bootstrap/lic/main.li`) by the C++ host. Full compiler rewrite in Li is out of scope for v1; this phase proves the build/run loop and CLI argv bridge.

**Architecture:** C++ `lic` remains the production compiler. Li bootstrap binary is a minimal CLI (`--version`, `smoke`) using `li_rt_argc` / `li_rt_argv` and libc `strcmp`. Parameterless `def main()` lowers to `li_user_main` with a C `main(argc, argv)` wrapper that calls `li_rt_set_args` before user code.

**Depends on:** Phase 5  
**Blocks:** Future full self-host (parser/types/codegen in Li)

## Development discipline (C++ first, then port to Li)

The C++ compiler stays the production and reference host even as the Li
compiler grows past this seed. Heavy changes follow the
**upgrade-li-from-cpp** skill:

1. **Find** the improvement — a parity-gate divergence, a bug surfaced while
   self-hosting, or a feature the next layer needs.
2. **Implement in C++** (`compiler/`) — the same code that ships in `lic`.
3. **Validate in C++**: pass the relevant `li-tests` suites *and* benchmark
   (tier1_micro / tier2_physics) with no regression.
4. **Upgrade the Li compiler from there** only once it passes tests and
   benchmarks well: port the validated logic into `bootstrap/lic/main.li` and
   re-pass the parity gates byte-identically.

Never prototype a heavy change directly in Li; never let the C++ and Li
implementations accumulate independent semantics.

---

## Task 1: Runtime argv bridge

**Files:**
- Modify: `runtime/li_rt.c`, `runtime/li_rt.h`

- [x] `li_rt_set_args(int argc, char** argv)`
- [x] `li_rt_argc() -> int`, `li_rt_argv(i: int) -> ptr`

---

## Task 2: Codegen + MIR fixes

**Files:**
- Modify: `compiler/codegen/emit.cpp`
- Modify: `compiler/mir/lower.cpp`

- [x] C `main` wrapper only for **parameterless** user `main` (preserves `typedict_ok.li` etc.)
- [x] `ptr` extern params/returns use `i8*` allocas (`ptr_locals`)
- [x] Nested call expressions as extern call args
- [x] `StoreInt`/`StoreI64` from ident or temp: set `rhs_is_literal = false` (MIR default was `true`)

---

## Task 3: Bootstrap source + script

**Files:**
- Create: `bootstrap/lic/main.li`
- Create: `scripts/bootstrap_lic.sh`
- Modify: `.gitignore` (`build/lic-from-li`)

- [x] CLI dispatches on `argv[1]` (`--version`, `smoke`, usage)
- [x] `bootstrap_lic.sh` builds and smoke-tests the binary

---

## Task 4: Tests + registration

**Files:**
- Modify: `li-tests/manifest.toml`

- [x] Integration `verify_ok` on `../bootstrap/lic/main.li`

---

## Exit gate

```bash
export LLVM_DIR="$(brew --prefix llvm@22)/lib/cmake/llvm"
./scripts/build.sh
./scripts/bootstrap_lic.sh
LIC=./build/compiler/lic/lic ./li-tests/run_all.sh
```

Expected:

- `lic 0.2.0-bootstrap (compiled with Li)` on `--version`
- `bootstrap: smoke ok`
- `li-tests: pass=46 fail=0`

---

---

## Layer 2 (2026-08-17): self-hosted lexer — DONE

`bootstrap/lic/main.li` now contains a real lexer (`lex_source`) written in Li,
reaching 100% token-stream parity with the C++ Lexer (`compiler/lexer`) across
the entire repo:

- **`lic lex <file>`** (C++ host): dev/parity command that dumps
  `kind<TAB>lexeme` per token, matching the C++ lexer's token enum ordinals.
- **Runtime seams** (`runtime/li_rt.c/h`): `li_rt_read_file` (whole-file,
  NUL-terminated), `li_rt_str_len`, `li_rt_str_char_at`, `li_rt_str_eq`,
  `bytes_slice_i`, `li_rt_emit_token` (emits one `kind<TAB>lexeme` line).
- **Lexer port**: full indentation state machine (Indent/Dedent stack, `=`
  body marker, `pending_indent_check`), comments, string literals, int/float/
  binary literals (including the C++ literal-suffix rule: `0.0f32` lexes as
  float `0.0` with the suffix consumed but excluded from the text), keywords,
  and the operator table. Quirks of the C++ lexer are replicated exactly:
  a `=` not followed by a newline is dropped, tabs are rejected, and
  `0b` without bits falls through to a number literal.
- **Parity gate**: `scripts/check_li_lexer_parity.sh` builds the Li lexer with
  the C++ host and diffs both token streams over an 8-file corpus plus the
  tab-indent error path. Full-repo sweep: **492/492 files exact token parity**
  (token kinds, lexemes, and accept/reject agreement).

Compiler gaps found and fixed while self-hosting the lexer:

- **Parser**: `if` statements now parse `elif` chains (desugared to nested
  `if`s) and `else` blocks into `Stmt::else_body` — the AST, MIR, typecheck,
  borrowck, and codegen already supported `else_body`; only the parser was
  missing it.
- **Codegen**: `MirOp::Jump` now skips dead branches when the current block
  already terminates (e.g. an `if` branch ending in `return`), fixing
  "Terminator found in the middle of a basic block!" for if/else, and the same
  latent bug in while/for loops whose bodies end in `return`.
- **Codegen**: `lower_print_arg` falls back to lowering general expressions
  (calls, binops) to a temp, so `print(f(x))` and `print(a + b)` print the
  value instead of silently emitting nothing.

## Remaining roadmap (full self-host)

- Layer 3: parser in Li — DONE (`parse` + `ast` subcommands, byte-exact AST
  parity via `scripts/check_li_ast_parity.sh`; 482-file full sweep wired into
  nightly CI).
- Layer 4: typecheck in Li — DONE for name resolution, type unification,
  contract well-formedness (E0301/E0302/E0303), array bounds (E0201), numeric/
  width mixing, protocol sizing, and effect checking (raises IO/Alloc/Net/Async).
  `scripts/check_li_check_parity.sh` now compares the full multiset of emitted
  error codes (not just accept/reject) across typecheck+generics+effects (35
  files). Still deferred: borrowck (E0310/E0311) and encapsulation
  visibility/trait/object policy — separate later layers.
- Layer 5: MIR lowering in Li. Follow `upgrade-li-from-cpp`:
  1. Add `lic mir <file>` to the C++ host — a canonical byte-exact dump of
     `lower_to_mir()` output (`compiler/mir/lower.cpp`, `mir.hpp`) as the
     parity reference.
  2. Implement the `mir` subcommand in `bootstrap/lic/main.li` (AST → MIR),
     diffing against the C++ dump.
  3. Gate with `scripts/check_li_mir_parity.sh` (skeleton in place).
- Layer 6: LLVM codegen in Li, or stage-2 bootstrap (Li-compiled `lic`
  compiling itself).
- `lic build` / `lic check` fully in bootstrap source
- **Agent skill for writing Li (post self-host).** Once the bootstrap
  compiler is self-hosted and the C++ host is only a reference (per the
  upgrade-li-from-cpp discipline), distill the language-authoring knowledge
  accumulated during the port into a proper agent skill
  (`.cursor/skills/write-li-language/SKILL.md`): the idiomatic way to write Li
  source that the compiler accepts — preferred syntax/type idioms, how to
  phrase contracts (requires/ensures/decreases) so VCs discharge, array index
  refinement patterns, effect/`raises` discipline, borrow rules, and the
  self-host parity-gate workflow for extending the compiler itself. This
  makes the language writable by agents (and documentable from one
  canonical source) now that the toolchain no longer needs the C++ host.
