# Phase 4: Runtime + Stdlib Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Provide builtins (`echo`), fixed arrays, `extern` FFI to C, and panic runtime so real programs link.

**Architecture:** Compiler intrinsics lower to `li_rt` or libc; `extern def` declares C symbols.

**Depends on:** Phase 3  
**Blocks:** Phase 5

---

### Task 1: `echo` builtin

**Files:**
- Modify: `crates/li_types/src/check.rs` (treat `echo` as builtin)
- Modify: `crates/li_mir/src/lower.rs`
- Modify: `runtime/li_rt.c`

- [ ] `echo` accepts `string` or `int` (format in rt)
- [ ] `li_rt_print_string`, `li_rt_print_int`

---

### Task 2: String literals

**Files:**
- Modify: lexer, parser, codegen

- [ ] NUL-terminated `*const i8` or `{ ptr, len }` — pick `{ ptr, len }` struct `StringLit` as immutable static
- [ ] v1: no heap strings; concatenation out of scope

---

### Task 3: Fixed-size arrays codegen

**Files:**
- Modify: `li_codegen/src/mir_llvm.rs`

- [ ] `array[N,T]` as LLVM array type on stack
- [ ] Literal index: compile-time OOB error (phase 2) + no runtime check
- [ ] Dynamic index: `icmp uge` + `li_bounds_fail`

---

### Task 4: `extern def` FFI

**Files:**
- Modify: parser, typechecker, codegen

```nim
extern def SDL_Init(flags: uint) -> int
```

- [ ] Parse `extern def` at module level
- [ ] Codegen as LLVM `declare` with C calling convention

---

### Task 5: Enums and objects

**Files:**
- Modify: MIR lower + codegen

- [ ] Enum: `i32` discriminant
- [ ] Object: LLVM struct with named fields, field index from type table

---

### Task 6: Example programs

**Files:**
- Create: `examples/hello.li` (final form with echo)
- Create: `examples/arrays.li`

- [ ] `lic build examples/hello.li -o hello && ./hello` prints `hello li`

---

### Phase 4 exit gate

- [ ] hello + arrays examples build and run
- [ ] `extern def` to `puts` from libc works in tiny test
