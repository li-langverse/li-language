# Phase 3: MIR + LLVM Codegen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Lower typed AST to MIR, optimize lightly, emit LLVM IR, link native binary via `lic build`.

**Architecture:** `li_mir` defines MIR ops; `li_codegen` lowers MIR → LLVM; driver shells out to `clang` for link.

**Tech Stack:** inkwell, clang

**Depends on:** Phase 2  
**Blocks:** Phase 4

---

## MIR instruction set (v1)

```
mir_fn ::= fn name(params) -> ret { basic_blocks }
bb     ::= label: mir_inst*
inst   ::= assign dest, operand
         | binop dest, op, lhs, rhs
         | br cond, then_bb, else_bb
         | jmp bb
         | ret operand?
         | call dest?, fn, args…
         | load_field / store_field
         | load_index / store_index (with bounds check call)
         | panic_if_oob index, len
```

---

### Task 1: MIR data structures

**Files:**
- Create: `crates/li_mir/src/lib.rs`
- Create: `crates/li_mir/src/lower.rs`

- [x] `lower_module(TypedModule) -> MirModule` (C++: `lower_to_mir` in `compiler/mir/lower.cpp`)
- [x] One basic block per stmt chain; if/while create CFG edges

---

### Task 2: MIR → LLVM

**Files:**
- Modify: `crates/li_codegen/src/lib.rs`
- Create: `crates/li_codegen/src/mir_llvm.rs`

- [x] Map MIR types to LLVM types (`array` → struct or stack array) (`compiler/codegen/emit.cpp`)
- [x] Emit `main` calling user `main` if present
- [x] Debug: `li_bounds_fail` call on dynamic index

---

### Task 3: Driver `lic build`

**Files:**
- Modify: `crates/lic/src/main.rs`
- Create: `crates/lic/src/driver.rs`

- [x] `lic build input.li -o out [--release]`
- [x] Pipeline: parse → check → mir → llvm bitcode → `clang -o out`
- [x] Link `runtime/li_rt.c`

---

### Task 4: End-to-end hello

**Files:**
- Create: `examples/hello.li`

```nim
def main() raises IO =
  echo "hello li"
```

- [x] Deferred until Phase 4 provides `echo` — for Phase 3 use: (superseded: Phase 4 shipped `echo`; `examples/hello.li` uses it)

```nim
def main() -> int =
  return 0
```

- [x] `lic build examples/hello.li -o hello && ./hello; echo $?` → 0

---

### Task 5: Snapshot tests

**Files:**
- Create: `crates/li_codegen/tests/llvm_snap.rs`

- [ ] insta snapshot of LLVM IR for `return 0` program

---

**Proof gaps (Doc-c):** [G-bnd](../../verification/provability-gaps.md#g-bnd) · [G-meta](../../verification/provability-gaps.md#g-meta)

### Phase 3 exit gate

- [x] Native binary from minimal proc
- [x] `-O2` flag forwarded to clang in `--release` (C++: `-O3 -march=native` in `compile.cpp` when `--release`)
- [x] Bounds check calls present in IR for dynamic index
