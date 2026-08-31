# Phase 4: Runtime + Stdlib Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Provide builtins (`echo`), fixed arrays, `extern` FFI to C, and panic runtime so real programs link.

**Architecture:** Compiler intrinsics lower to `li_rt` or libc; `extern proc` declares C symbols.

**Depends on:** Phase 3  
**Blocks:** Phase 5

---

### Task 1: `echo` builtin

**Files:**
- Modify: `crates/li_types/src/check.rs` (treat `echo` as builtin)
- Modify: `crates/li_mir/src/lower.rs`
- Modify: `runtime/li_rt.c`

- [x] `echo` accepts `string` or `int` (format in rt)
- [x] `li_rt_print_string`, `li_rt_print_int` (`runtime/li_rt.c`: `li_rt_print_str`, `li_rt_print_int`)

---

### Task 2: String literals

**Files:**
- Modify: lexer, parser, codegen

- [x] NUL-terminated `*const i8` or `{ ptr, len }` — pick `{ ptr, len }` struct `StringLit` as immutable static (v1: C string literals)
- [x] v1: no heap strings; concatenation out of scope

---

### Task 3: Fixed-size arrays codegen

**Files:**
- Modify: `li_codegen/src/mir_llvm.rs`

- [x] `array[N,T]` as LLVM array type on stack
- [x] Literal index: compile-time OOB error (phase 2) + no runtime check
- [x] Dynamic index: `icmp uge` + `li_bounds_fail`

---

### Task 4: `extern proc` FFI

**Files:**
- Modify: parser, typechecker, codegen

```nim
extern proc SDL_Init(flags: uint) -> int
```

- [x] Parse `extern proc` at module level
- [x] Codegen as LLVM `declare` with C calling convention

---

### Task 5: Enums and objects

**Files:**
- Modify: MIR lower + codegen

- [x] Enum: `i32` discriminant
- [x] Object: LLVM struct with named fields, field index from type table

---

### Task 6: Example programs

**Files:**
- Create: `examples/hello.li` (final form with echo)
- Create: `examples/arrays.li`

- [x] `lic build examples/hello.li -o hello && ./hello` prints `hello li`

---

### Phase 4 exit gate

- [x] hello + arrays examples build and run (`examples/hello.li`, `examples/arrays.li`)
- [x] `extern proc` to `puts` from libc works in tiny test (`examples/tetris/` SDL rt)
