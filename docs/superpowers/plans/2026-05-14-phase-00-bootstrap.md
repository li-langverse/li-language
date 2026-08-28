# Phase 0: Bootstrap Implementation Plan (C++)

> **For agentic workers:** Use `.cursor/skills/build-li-master-plan/SKILL.md` — continue through exit gate without per-step user prompts.

**Goal:** CMake + Ninja workspace, C++ compiler library skeleton, LLVM 18 smoke test, `lic` CLI, C runtime stubs.

**Architecture:** Static libs under `compiler/` (`lexer` → … → `codegen`); thin `lic` executable; `runtime/li_rt.c`.

**Tech stack:** C++17, CMake 3.20+, Ninja, **LLVM 18** (sole backend)

**Depends on:** Nothing  
**Blocks:** Phase 1

---

### Task 1: Root CMake + compiler layout

**Files:**
- `CMakeLists.txt` (repo root)
- `compiler/CMakeLists.txt` + stub libs: `diagnostics`, `lexer`, `parser`, `ast`, `types`, `mir`, `codegen`, `lic`
- `.gitignore`

**Verify:**
```bash
cmake -B build -G Ninja -DLLVM_DIR="$LLVM_DIR"
cmake --build build
```
Expected: all static libs + `build/compiler/lic/lic` link.

---

### Task 2: LLVM smoke test

**Files:**
- `compiler/codegen/smoke_llvm.cpp` — emit and verify `int main() { return 0; }`
- `compiler/lic/main.cpp` — `lic smoke-llvm` subcommand

**Verify:**
```bash
./build/compiler/lic/lic smoke-llvm
```
Expected: `smoke-llvm: ok (main returns 0)`

---

### Task 3: C runtime skeleton

**Files:**
- `runtime/li_rt.c`, `runtime/li_rt.h` — `li_panic`, `li_bounds_fail`

**Verify:** `cmake --build build` still succeeds.

---

### Task 4: Wire test harness path

**Files:**
- `li-tests/manifest.toml` — `lic = "../build/compiler/lic/lic"`

---

## Phase 0 exit gate

- [ ] `cmake --build build` succeeds
- [ ] `./build/compiler/lic/lic smoke-llvm` succeeds
- [ ] Master plan Phase 0 checkbox checked

## Prerequisites

```bash
brew install llvm@18 cmake ninja
export LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"
```

On Linux, install `llvm-18-dev` and set `LLVM_DIR` to the CMake package path.
