# Phase 6: Self-host seed

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or build-li-master-plan.

**Goal:** Ship a **bootstrap** `lic` binary compiled from Li source (`bootstrap/lic/main.li`) by the C++ host. Full compiler rewrite in Li is out of scope for v1; this phase proves the build/run loop and CLI argv bridge.

**Architecture:** C++ `lic` remains the production compiler. Li bootstrap binary is a minimal CLI (`--version`, `smoke`) using `li_rt_argc` / `li_rt_argv` and libc `strcmp`. Parameterless `proc main()` lowers to `li_user_main` with a C `main(argc, argv)` wrapper that calls `li_rt_set_args` before user code.

**Depends on:** Phase 5  
**Blocks:** Future full self-host (parser/types/codegen in Li)

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
export LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"
./scripts/build.sh
./scripts/bootstrap_lic.sh
LIC=./build/compiler/lic/lic ./li-tests/run_all.sh
```

Expected:

- `lic 0.2.0-bootstrap (compiled with Li)` on `--version`
- `bootstrap: smoke ok`
- `li-tests: pass=46 fail=0`

---

## Out of scope (later)

- Rewrite lexer/parser/types/MIR/codegen in Li
- Stage-2 bootstrap: Li-compiled `lic` compiling itself
- `lic build` / `lic check` in bootstrap source
