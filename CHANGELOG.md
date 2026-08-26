# Changelog

All notable changes to Li are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versioning follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Self-hosted parser (Layer 2 of the Li front end):** `bootstrap/lic/main.li` now parses Li in Li — full recursive-descent port of the C++ parser's grammar (module decls, decorators, visibility, type aliases incl. typedict/enum/trait/object, array/tuple/simd/Callable/refinement types, procs with contracts + raises, if/elif/else, while, for, parallel-for, borrow/discard, imports, error decls, theorems) — gated by `scripts/check_li_parser_parity.sh`: 20-file accept corpus (token-stream parity vs `lic lex`), 5-file reject corpus, and a full-repo sweep mode at **482/482 exact accept/reject parity** with the C++ parser; `parse <file>` subcommand re-dumps the consumed token stream and reports `parse ok` / `parse error`. New `CallProc` lowering coerces pointer-width args (`ptr→i64` / `i64→ptr`) so string literals can pass into user-defined helpers — [2026-08-18-self-hosted-parser.md](docs/release-notes/2026-08-18-self-hosted-parser.md).

- **Runtime net `-Werror` hygiene gate:** `scripts/check-runtime-net-werror.sh` compiles `runtime/li_rt_net.c` with `-Werror -Wall -Wextra` in a dedicated CI phase (catches macOS `#else` stub regressions at compile time); `ptr_i` no longer drops `const`, and Linux-only dead statics are marked `LI_RT_NET_UNUSED` — [2026-08-18-self-hosted-parser.md](docs/release-notes/2026-08-18-self-hosted-parser.md).

- **Interior-bounds ensures on unrolled physics kernels:** `diffuse_explicit` (`li-physics-weather`) and `poisson_jacobi_step` (`li-physics-em`) now ensure the unrolled kernel writes only interior cells 1..6 via `diffuse_step_cell`/`poisson_step_cell` constant-index helpers, all discharged by the native VC engine — [2026-08-18-self-hosted-parser.md](docs/release-notes/2026-08-18-self-hosted-parser.md).

- **Nightly full standalone sweep:** `ci.sh` runs the 20-package standalone subset for fast CI and the full workspace-member sweep under `LI_NIGHTLY=1` (entrypoint `scripts/ci-nightly.sh`), so every member package is verified to install, build, and run outside the monorepo nightly — [2026-08-18-self-hosted-parser.md](docs/release-notes/2026-08-18-self-hosted-parser.md).

- **Compiler ↔ package carve-out, enforced by CI:** the compiler (`compiler/`, `runtime/`, `bootstrap/`) and `std/` facades are now verified upstream-only — no `packages/` references in source or CMake, no package imports from `std/`, every package top-level with sibling-only `path = "../<name>"` deps — and every package is verified to install, build, and run standalone in a fresh tree with its transitive dependency closure (`scripts/check-compiler-isolation.sh`, `scripts/check-package-standalone.sh`, wired into `ci.sh`) — [2026-08-18-compiler-package-isolation.md](docs/release-notes/2026-08-18-compiler-package-isolation.md).

- **`li-nanoreactor` package (standalone, installable):** ab initio nanoreactor as pure-Li modules — box/piston + thermal cycling (A), LJ + Morse pair potentials (B), velocity-Verlet integrator + thermostat (C), squared-distance bond/event detection (D), a reactor driver (E) plugging them together, and common sample systems (water dimer, methane, H2+O2 — F). Each module is unit-tested by itself (`li-tests/unit/`), then exercised end-to-end per sample system (`li-tests/e2e/`), with a runnable example (`examples/nanoreactor_run.li`) — [2026-08-17-nanoreactor-package.md](docs/release-notes/2026-08-17-nanoreactor-package.md).

### Fixed

- **`lic check` now surfaces warnings on success:** human-mode `lic check` previously printed diagnostics only on failure, so advisory warnings (W0403 typosquat, W0401/W0402, N0401) were invisible behind a green exit code; it now renders warnings/notes on success too and caches them, while exit code stays 0 unless `--deny-warnings` — [2026-08-17-decorator-typosquat-modern.md](docs/release-notes/2026-08-17-decorator-typosquat-modern.md).

- **`resolve_imports` no longer fails on warnings:** it returned `diags.empty()` instead of `!diags.has_errors()`, so any warning accumulated before import resolution (e.g. the policy W0403) made the frontend return false and every command exit 1 — [2026-08-17-decorator-typosquat-modern.md](docs/release-notes/2026-08-17-decorator-typosquat-modern.md).

- **Float-array element arithmetic in codegen:** `a[0] * b[0]` on `array[N, float]` was lowered through int MIR (`fptosi` truncation) because `is_float_expr` did not recognize `Index` reads (or object array fields); user-defined float-returning calls with array params then returned uninitialized stack memory. `is_float_expr` now resolves array-element, object-array-field, and nested matrix index reads, float-array params are seeded, and a new runtime gate (`li-tests/tooling/check_float_array_codegen_runtime.sh`, wired into CI) asserts the real computed values — [2026-08-17-nanoreactor-package.md](docs/release-notes/2026-08-17-nanoreactor-package.md).

### Changed

- **`sqrt_open_bound.li` now emits valid Lean while staying open:** the sqrt `abs` lemma's AutoVC def binds `result` as a formal (fixing "Unknown identifier `result`") and no closing theorem is emitted, so the VC is still reported open (per `contracts_discharge_corpus.sh`) but `lake build AutoVC` compiles. Manifest outcome changed to `compile_open_ok`; contracts_verify is now **30/30** — [2026-08-17-float-certs-and-sqrt.md](docs/release-notes/2026-08-17-float-certs-and-sqrt.md).

- **Float requires enforcement now matches ints (E0304):** the typechecker tracks float const locals, `a[i] = c` / `a[i][j] = c` stores, unrolls simple counter loops, and folds call-assignments (`s = dot4_float(a, b)`) through the callee `ensures`, so a wrong float call value fails at typecheck like ints instead of leaving an unchecked VC — [2026-08-17-float-certs-and-sqrt.md](docs/release-notes/2026-08-17-float-certs-and-sqrt.md).

- **`if`/`elif`/`else` chains and `print(<expr>)` fixed:** the parser now parses `elif` chains (desugared to nested `if`s) and `else` blocks — the AST/MIR/typecheck/codegen already supported `else_body`, only the parser was missing it. Codegen drops dead `Jump`s after a terminating branch (fixes "Terminator found in the middle of a basic block!" for if/else and loops ending in `return`), and `print` lowers general expressions (calls, binops) to a temp so `print(f(x))` prints the value instead of nothing — [2026-08-17-self-hosted-lexer.md](docs/release-notes/2026-08-17-self-hosted-lexer.md).

### Added

- **Finished 2D float matmul value certificate** (`linalg_mat2_float_value.li`, `prove_lean_ok`): `C = mat2_mul(A, B)` on `array[2, array[2, float]]` tiles folds every cell to a constant (19/22/43/50) — nested `a[i][j]` element stores are canonicalized, and the callee `ensures` conjunction (`result[i][j] == …`) is constant-folded per cell at the call site. A negative guard (`linalg_mat2_float_wrong_value.li`, `compile_fail` E0304) proves the fold is real — [2026-08-17-float-certs-and-sqrt.md](docs/release-notes/2026-08-17-float-certs-and-sqrt.md).

- **Finished float dot4 certificate:** the fixed-size dot now proves all three forms in `li-tests/contracts_verify/linalg_dot4_float_closed.li` with zero open obligations — `return x @ y` equals the expanded sum (new `@`-on-arrays witness), a handwritten `acc = acc + x[i] * y[i]` loop equals it too (loop witness generalized from hardcoded `a`/`b`/`acc`/`i` to any names), and `s = dot4_float(a, b)` with `a = 1.0`/`b = 2.0` folds to `s == 8.0` (counter loops are unrolled so `a[i] = 1.0` stores are tracked per element, float const locals + float array-element stores feed a new `fold_const_locals`, and call-assignments fold the callee `ensures` to a constant at the call site) — [2026-08-17-dot4-float-certificate.md](docs/release-notes/2026-08-17-dot4-float-certificate.md).

- **Self-hosted lexer (Layer 2 of the Li front end):** `bootstrap/lic/main.li` now lexes Li in Li — full indentation state machine, comments, strings, int/float/binary literals (including C++ literal-suffix rules), keywords, and operators — with **492/492 exact token-stream parity** across the repo vs the C++ Lexer. New `lic lex <file>` token-dump command, runtime seams (`li_rt_read_file`, `li_rt_str_len/char_at/eq`, `bytes_slice_i`, `li_rt_emit_token`), and the `scripts/check_li_lexer_parity.sh` gate — [2026-08-17-self-hosted-lexer.md](docs/release-notes/2026-08-17-self-hosted-lexer.md).

### Changed

- **`extern def` canonical for FFI:** `extern proc` is rejected in favor of `extern def` (bare `proc` remains legacy). The parser now accepts `extern def`, the migrator converts `extern proc` → `extern def`, `check-li-def-syntax.sh` flags both, and a rejection test (`extern_proc_syntax_rejected`) guards the new rule — [2026-08-17-extern-def-ffi-float-cmp-aimd.md](docs/release-notes/2026-08-17-extern-def-ffi-float-cmp-aimd.md).

- **`print` replaces the `echo` keyword:** the `echo <expr>` keyword statement is gone; output is now the `print(...)` builtin call (modern-Python style). `echo` is no longer reserved, the lexer/parser keyword and its MIR special-casing were removed, typecheck/prelude/borrowck/mir now key on `print`, and all `.li` sources + docs + the `stdlib_seal` shadow tests (`shadow_print*`) were migrated.

### Added

- **`var`-object in-out ABI write-back:** `var Reader`/`var RigidBody`-style object params are now passed by reference (pointer args + callee stores through them) instead of by value with lost mutations; `bytes/reader_writer_smoke.li` now exits 0 at runtime.

- **Object-arg flattening at call sites:** `push_mir_args_for_object_value` no longer silently drops object-typed arguments that aren't bare idents — call-returned objects (`gui_viewport_selection_none()`) and field-access objects (`layout.viewport`) now flatten into the callee's expected leaf slots (was under-pushing 0/N or 26/32 args, which the LLVM verifier rejected).

- **`ReturnVoid` emits `null` for pointer-returning procs:** `ret i0 0` (a `ConstantInt` on a pointer type) broke `str`-returning procs; pointer return types now return `ConstantPointerNull`.

- **Borrowck no longer moves scalars on call:** non-`var` ident args of scalar type are copied by value in the ABI, so `double(a); double(a)` is legal; objects/arrays still move (E0311 unchanged) — `use_after_move.li` still passes.

- **`const_float_locals` is cleared per proc:** float consts (e.g. `h = 18.0`) from one proc leaked into the next, folding unrelated `requires w > 0` call checks to `0 > 0` (E0304 false positives) across a whole module.

- **`lig.present` package + composable suite closure:** new `li-lig-present` workspace package (`import_name = "lig.present"`) wrapping the runtime present surface; `li-studio` depends on it and reuses `physics.rigid` instead of redefining `RigidBody`. The composable suite went **16/16** (`import_studio_*`, `import_render_wgpu_fps`, `import_lig_kernel` now compile; the lig lib gained the kernel externs `li_rt_lig_kernel_run` / `li_rt_lig_kernel_last_validity_ratio` + `lig_kernel_matmul_f32` / `lig_kernel_run_auto` / `lig_validity_gate_pass`).

- **`li-aimd` package (standalone, installable):** ab initio molecular dynamics — DFT-coupled MD energy-drift oracles, thermostatted integrators, grand-canonical charge-neutrality, and headless batch runners — as its own package with `[dependencies]` on `li-chem` and `li-sim` (not nested under either) and smokes registered in the monorepo manifest — [2026-08-17-extern-def-ffi-float-cmp-aimd.md](docs/release-notes/2026-08-17-extern-def-ffi-float-cmp-aimd.md).

- **Li theorem layer (Layer 1):** `axiom` / `theorem` / `lemma` declarations in li — lexer/parser/AST (`TheoremDecl`), proposition typecheck (E0505), `lic verify` axiom/theorem/lemma counts; math axioms ported from `MathAxioms.lean` into `proof-db/math/axioms/*.li` (`peano_zero_not_succ`, `peano_succ_injective`, `order_trichotomy_nat`, `order_antisym`, `real_add_comm`, `real_add_assoc`, `real_mul_distrib`, `real_mul_one`) — [2026-08-14-li-theorem-layer.md](docs/release-notes/2026-08-14-li-theorem-layer.md).
- **Native proof discharge (Layer 2):** `lic` now closes `theorem`/`lemma` propositions itself — rewriting engine in `compiler/verify/vc_prove.{hpp,cpp}` (AC normalization, distributivity, identity rules, boolean simplification, order transitivity over assumed facts); `lic verify` reports `theorems_proved=`/`theorems_open=`, and `lic build` fails on non-discharged theorems unless `--allow-open-vc`; `proof-db/math/lemmas/ring_discharge.li` (17 closed lemmas, 0 open); array-index helpers (`arr[helper(args)]` with bounds `ensures`) + literal-range witness unblock `examples/tetris/main.li`'s E0201s — [2026-08-14-native-discharge-layer2.md](docs/release-notes/2026-08-14-native-discharge-layer2.md).

### Fixed

- **Float comparison codegen:** runtime float comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`) were emitted as integer compares after `FPToSI` truncation (so `0.9 <= 0.8` was true); comparisons are now emitted as `FCmp` (with int operands widened via `SIToFP`) and produce a proper i32 0/1 result — [2026-08-17-extern-def-ffi-float-cmp-aimd.md](docs/release-notes/2026-08-17-extern-def-ffi-float-cmp-aimd.md).

- **Native VC float constant folding:** the native discharge engine folded only bare literals, so `ensures result == 3.0` stayed open against `return 1.0 + 1.0 + 1.0`; `fold_const` now reduces `+ - * /` over numeric literals (exact int64, IEEE double for floats, overflow/div-by-zero left unfolded) and the witness layer uses `fold_numeric_equal` to close `result == <const>` — `physics/golden_positions_sum.li` now discharges strictly — [2026-08-17-extern-def-ffi-float-cmp-aimd.md](docs/release-notes/2026-08-17-extern-def-ffi-float-cmp-aimd.md).

- **Constant array-element store witness:** `result == 7` stayed open against `return read_at(a, 6)`; the proof-fact collector now records `a[i] = c` stores and folds `a[c]` back, and the witness layer substitutes a callee's `ensures result == a[i]` to close the caller's `ensures result == <const>` — `contracts_verify/bounds_refinement_release_ok.li` now discharges (leaving only the intentional `sqrt_open_bound.li` Float-lemma gap) — [2026-08-17-extern-def-ffi-float-cmp-aimd.md](docs/release-notes/2026-08-17-extern-def-ffi-float-cmp-aimd.md).

- **Missing-contract sweep:** all repo `.li` files pass the E0301/E0302 contract gate — tier-2 physics benchmarks, tetris demo externs, physics proof-db (`total_momentum`), and std stubs (`io`/`csv`/`ui`) now declare `requires`/`ensures` (plus `raises IO`/`raises Alloc` where effects require them) — [2026-08-14-missing-contracts-fix.md](docs/release-notes/2026-08-14-missing-contracts-fix.md).

- **PH-UX vertical gap #1:** Studio UI bench registry and `bench-studio-viewport-perf.sh` reference `packages/lig` (`wgpu_smoke` hook) instead of removed `packages/li-gpu` — [2026-05-25-vertical-gap-bench-lig.md](docs/release-notes/2026-05-25-vertical-gap-bench-lig.md).

### Added

- **G-proof-db (gap2):** three `L-MATH-*` catalog registrations + `proof-db-gap2-report.sh` — [2026-05-25-gap2-proof-db.md](docs/release-notes/2026-05-25-gap2-proof-db.md).
- **Studio MCP gap #6/#7 (contracts):** eight tool IDs (`am_export_print`, `chem_dft_run`, `studio_adaptive_layout`), `studio_mcp_tool_dispatch` stub, `li-chem` `chem_dft_run_smoke`, smokes `studio_mcp_extended.li` / composable chem — [2026-05-25-vertical-gap-mcp-chem.md](docs/release-notes/2026-05-25-vertical-gap-mcp-chem.md).

- **Vertical gap #4/#9 sim step physics** — `sim_scientific_tick_stub`, `studio_game_step_hook`, `studio_md_particle_tier_select_ok`, smokes `studio_sim_step_by_profile.li` / `import_studio_sim_step_by_profile.li` — [2026-05-25-vertical-gap-sim-step-physics.md](docs/release-notes/2026-05-25-vertical-gap-sim-step-physics.md).

- **Vertical gap #2/#10 native present (partial):** `STUDIO_DEMO_PROFILE` env wiring, `li_rt` lig host present restore, `li-studio-demo` verticals capture preference — [2026-05-26-vertical-gap-native-present.md](docs/release-notes/2026-05-26-vertical-gap-native-present.md).

### Added

- **PH-SIM vertical gap #3:** domain profile stubs `li-sim-automotive`, `li-sim-robotics`, `li-sim-additive`, `li-sim-drug-design` (`import sim.*`, contract + studio id constants, `lic check` smokes) — [2026-05-26-vertical-sim-domain-stubs.md](docs/release-notes/2026-05-26-vertical-sim-domain-stubs.md).

- **PH-UX vertical gap #5:** Full `lig-kernels.toml` catalog rows (`md_force_short`, `heat_stencil_2d_f32`, …), `cuda`/`hip`/`metal` = `N/A` until `LIG_EMIT_*`, parity harness emits all `kernel_ids` — [2026-05-25-vertical-gap-lig-kernels.md](docs/release-notes/2026-05-25-vertical-gap-lig-kernels.md).


- **PH-HW integration (`lig` + studio gap):** Rollup for multi-vendor GPU work packages WP1–WP5 ([#217](https://github.com/li-langverse/lic/pull/217), [#218](https://github.com/li-langverse/lic/pull/218), [#213](https://github.com/li-langverse/lic/pull/213), [#220](https://github.com/li-langverse/lic/pull/220), [#222](https://github.com/li-langverse/lic/pull/222)) and merged `studio-gap-close-wave1` (UX/MCP/sim/world) on `feat/ph-hw-multi-vendor` — [2026-05-25-lig-ph-hw-integration.md](docs/release-notes/2026-05-25-lig-ph-hw-integration.md).

- **2i / G-math (tracker):** `norm_non_array.li` compile_fail for scalar `norm` — `docs/release-notes/2026-05-25-2i-norm-plan-tracker.md`.

### Added

- **Gap closure queue (Phase 2a audit):** `docs/verification/GAP_CLOSURE_QUEUE.md` — prioritized open gaps excluding open PRs — [2026-05-25-gap-closure-queue.md](docs/release-notes/2026-05-25-gap-closure-queue.md).

### Added

- **G-dec (partial):** `check_mir_vectorized_decorator.sh` in `contracts_discharge_corpus.sh` + master-plan gates; provability-gaps **G-dec** / **P-dec** sync — `docs/release-notes/2026-05-25-g-dec-gap-close-corpus.md`.

### Added

- **Vertical gap #4/#9 sim step physics** — `sim_scientific_tick_stub`, `studio_game_step_hook`, `studio_md_particle_tier_select_ok`, smokes `studio_sim_step_by_profile.li` / `import_studio_sim_step_by_profile.li` — [2026-05-25-vertical-gap-sim-step-physics.md](docs/release-notes/2026-05-25-vertical-gap-sim-step-physics.md).

### Fixed

- **PH-UX vertical gap #1:** Studio UI bench registry and `bench-studio-viewport-perf.sh` reference `packages/lig` (`wgpu_smoke` hook) instead of removed `packages/li-gpu` — [2026-05-25-vertical-gap-bench-lig.md](docs/release-notes/2026-05-25-vertical-gap-bench-lig.md).

### Added

- **P-physics proof database:** `docs/verification/proof-database/entries/physics-*.toml` (`P-AX-*`, `P-LM-*`); tier-2 bench refs; scalar lemmas in `Discharge.lean` — [2026-05-25-proof-db-physics-axioms.md](docs/release-notes/2026-05-25-proof-db-physics-axioms.md).
- **Proof-db sweep reporter:** `scripts/proof-db-report.sh`, `proof-db/expected.json`, `discrepancies.toml`, `reporter.md` — [2026-05-25-proof-db-sweep-reporter.md](docs/release-notes/2026-05-25-proof-db-sweep-reporter.md).

### Added

- **Proof DB lemma rebuild:** `scripts/proof-db/rebuild_lemmas.sh` → `data/proof-db/latest-report.{json,md}` from `docs/verification/proof-database/entries/` — [2026-05-25-proof-db-rebuild-pipeline.md](docs/release-notes/2026-05-25-proof-db-rebuild-pipeline.md).
- **Proof-db discrepancy analyzer:** `scripts/proof-db/compare_reference.py`, `proof-database/DISCREPANCIES.md` — [2026-05-25-proof-db-discrepancies.md](docs/release-notes/2026-05-25-proof-db-discrepancies.md).
- **Proof-db CI release gate:** `proof-db/baseline.jsonl`, `scripts/check-proof-db.sh`, advisory `LI_PROOF_DB_STRICT` in `scripts/ci.sh` — [2026-05-25-proof-db-ci-gate.md](docs/release-notes/2026-05-25-proof-db-ci-gate.md).
- **Proof-db sweep reporter:** `scripts/proof-db-report.sh`, `proof-db/expected.json`, `discrepancies.toml`, `reporter.md` — [2026-05-25-proof-db-sweep-reporter.md](docs/release-notes/2026-05-25-proof-db-sweep-reporter.md).

### Added

- **Classical math proof database:** `docs/semantics/proof-db/math/`, `docs/verification/proof-database/entries/math-*.toml` (`M-AX-*`, `M-LM-*`), `lake build ProofDbMath` — [2026-05-25-proof-db-math-axioms.md](docs/release-notes/2026-05-25-proof-db-math-axioms.md).

### Added

- **Execution surface docs:** specs `2026-05-25-li-execution-surface.md`, `2026-05-25-li-execution-resources.md`; handbook `docs/language/parallelism.md` — [2026-05-25-execution-surface-docs.md](docs/release-notes/2026-05-25-execution-surface-docs.md).
- **Proof DB lemma rebuild:** `scripts/proof-db/rebuild_lemmas.sh` → `data/proof-db/latest-report.{json,md}` — [2026-05-25-proof-db-rebuild-pipeline.md](docs/release-notes/2026-05-25-proof-db-rebuild-pipeline.md).
- **Proof-db discrepancy analyzer:** `scripts/proof-db/compare_reference.py`, `proof-database/DISCREPANCIES.md` — [2026-05-25-proof-db-discrepancies.md](docs/release-notes/2026-05-25-proof-db-discrepancies.md).
- **Proof database (v0):** `docs/verification/proof-database.md`, `proof-db/` manifest (axioms/lemmas + `release_pin`), `scripts/check-proof-db.sh` CI smoke — [2026-05-25-proof-database-arch.md](docs/release-notes/2026-05-25-proof-database-arch.md).
- **P-physics proof database:** `docs/verification/proof-database/entries/physics-*.toml` (`P-AX-*`, `P-LM-*`); tier-2 bench refs; scalar lemmas in `Discharge.lean` — [2026-05-25-proof-db-physics-axioms.md](docs/release-notes/2026-05-25-proof-db-physics-axioms.md).
- **G-trust (Partial+):** **T-GetElem** (`typing_getElem`) in `docs/semantics/Core.lean` — [2026-05-25-g-trust-core-getelem.md](docs/release-notes/2026-05-25-g-trust-core-getelem.md).

### Added

- **Proof DB (2f slice):** `proof-db/index.json` + `proof-db/lean/ProofDB.lean` — five standard lemmas (4 proved, 1 `sorry`); `lake build ProofDB` — [2026-05-25-proof-db-lean-bridge.md](docs/release-notes/2026-05-25-proof-db-lean-bridge.md).

### Added

- **Proof DB (2f slice):** `proof-db/index.json` + `proof-db/lean/ProofDB.lean` — five standard lemmas (4 proved, 1 `sorry`); `lake build ProofDB` — [2026-05-25-proof-db-lean-bridge.md](docs/release-notes/2026-05-25-proof-db-lean-bridge.md).

### Added

- **Proof DB (2f slice):** `proof-db/index.json` + `proof-db/lean/ProofDB.lean` — five standard lemmas (4 proved, 1 `sorry`); `lake build ProofDB` — [2026-05-25-proof-db-lean-bridge.md](docs/release-notes/2026-05-25-proof-db-lean-bridge.md).

### Changed

- **Plan checkboxes (wave):** sync `docs/superpowers/plans/*.md` exit gates with shipped Phases 0–5, Pkg, P-linalg loop witness, and C++ compiler evidence — [2026-05-25-plan-checkbox-audit-wave.md](docs/release-notes/2026-05-25-plan-checkbox-audit-wave.md).
- **LLVM toolchain:** pin **22** (was 18) — `scripts/llvm-env.sh`, `scripts/ci-install-llvm.sh`, CMake gate — [2026-05-22-llvm-22-toolchain-bump.md](docs/release-notes/2026-05-22-llvm-22-toolchain-bump.md).

### Added

- **G-test-verify Done:** `prove_lean_ok` in `li-tests/run_all.sh`; 14 closed `contracts_verify` specimens — `docs/release-notes/2026-05-25-g-test-verify-prove-lean-ok.md`.

- **Ecosystem phase 0:** `algorithms-and-libraries-plan.md`, `lic-ecosystem-baseline.md`, agent skill `run-local-ci-gha-quota` — `docs/release-notes/2026-05-22-lic-ecosystem-phase0-baseline.md`.
- **2i broadcast (partial):** `array[1, T]` element-wise broadcast to `array[N, T]` — `docs/release-notes/2026-05-22-2i-broadcast-len1.md`.
- **P-float (partial):** `sqrt_open_bound.li` calls `li_rt_sqrt`; tight `abs` ensures still open — `docs/release-notes/2026-05-22-p-float-sqrt-runtime.md`.
- **7d/7e (partial):** `@parallel(disjoint=)` on `def` inherits to inner `parallel for`; tier-1 `bench.py` uses `--allow-open-vc` — `docs/release-notes/2026-05-22-7d-7e-bench-parallel.md`.
- **Tier-1 matmul benches:** hoist `A`/`B` init out of hot loop in `matmul_naive` / `matmul_blocked` Li drivers — `docs/release-notes/2026-05-22-tier1-matmul-bench-hotloop.md`.

### Fixed

- **CI `test-auth-bearer`:** `build-li-httpd.sh` links `main.li` so `li-httpd` runs `httpd_run_from_argv` (was stub `main` returning 0) — [2026-05-25-ci-test-auth-bearer-main-li.md](docs/release-notes/2026-05-25-ci-test-auth-bearer-main-li.md).

- **HTTPd M1 bearer auth gate:** non-Linux `epoll_ctl_add_listen_i` stub, `build-li-httpd.sh`, plan gates run `test-auth-bearer.sh` on `build/li-httpd` — [2026-05-22-httpd-m1-bearer-auth-gate.md](docs/release-notes/2026-05-22-httpd-m1-bearer-auth-gate.md).
- **HTTPd routing CI:** rebase plan-loop branch on `main`; `run_httpd_config.sh` — `--allow-open-vc` + `HTTPD_SKIP_LI_ROUTING_BIN` — `docs/release-notes/2026-05-22-httpd-rebase-main-post-164.md`.
- **G-lean / P-linalg:** `mat2_at2_float_spec_proved` — closed via `mat2_at2_eval` + `rfl` (no `sorry`); AutoVC ensures use eval not free `result` — `docs/release-notes/2026-05-22-mat2-float-spec-closed.md`.
- **MIR BinOpInt literals:** `rhs_is_literal` default no longer makes `r != 1` compare to `0`; `lic build --allow-open-vc <file> -o <out>` accepts flags before the input path — `docs/release-notes/2026-05-22-binop-int-literal-ne-fix.md`.

### Changed

- **Proof CLI flags:** `lic build` / `lic verify` use `--allow-open-vc` and `--no-lean-verify` instead of `LI_ALLOW_OPEN_VC` / `LI_BUILD_VERIFY_LEAN*` env bypasses (env vars ignored with warning).

### Added

- **HTTPd autonomous plan loop:** `scripts/httpd-plan-loop.py`, `httpd-plan-gates.sh`, baseline doc, goal-directed `code_implementer` via li-cursor-agents — [2026-05-22-httpd-plan-autonomous-loop.md](docs/release-notes/2026-05-22-httpd-plan-autonomous-loop.md).
- **HTTPd M1 core (rate limits):** `limits.rate_limit_rps` required for `proxy:` routes in Python validator + desugar; goal-directed `code_implementer` plan loop — [2026-05-22-httpd-m1-core-rate-limits.md](docs/release-notes/2026-05-22-httpd-m1-core-rate-limits.md).
- **HTTPd M1 ingress headers:** route-key header extras must match ingress allowlist; reject `x-upstream-*` / hop-by-hop — [2026-05-22-httpd-m1-ingress-headers.md](docs/release-notes/2026-05-22-httpd-m1-ingress-headers.md).

- **HTTPd M1 TOML desugar:** `li-tests/config_desugar/` goldens (prefix_strip, header extras), `check-httpd-config-desugar.sh`, C runtime route-key extras — [2026-05-22-httpd-m1-toml-desugar.md](docs/release-notes/2026-05-22-httpd-m1-toml-desugar.md).
- **HTTPd M1 routing tests:** `li-tests/routing/` table cases, `config_reject/routing_overlap.toml`, `run_routing.sh`, green `match_routes.li` gate — [2026-05-22-httpd-m1-routing-tests.md](docs/release-notes/2026-05-22-httpd-m1-routing-tests.md).
- **HTTPd autonomous plan loop:** `scripts/httpd-plan-loop.py`, `httpd-plan-gates.sh`, baseline doc, `httpd_implementer` agent — [2026-05-22-httpd-plan-autonomous-loop.md](docs/release-notes/2026-05-22-httpd-plan-autonomous-loop.md).
- **HTTPd M1 plan continue:** overlap reject, `validate-httpd-config` / `flatten-httpd-config`, Bearer 401 runtime + example — [2026-05-22-httpd-m1-plan-continue.md](docs/release-notes/2026-05-22-httpd-m1-plan-continue.md).
- **P-loop (2f):** Close `linalg_dot4_int_loop_open` AutoVC via static loop witness; `Li.Discharge.dot4_int_loop_eval_spec` — `docs/release-notes/2026-05-22-p-loop-dot-closed.md`.
- **li-log M1:** `packages/li-log`, `runtime/li_rt_log.c` access sink + redaction; `li-tests/log/redact_bearer.li` — [2026-05-22-li-log-m1-package.md](docs/release-notes/2026-05-22-li-log-m1-package.md).
- **HTTPd M1 static recv:** serve files without mandatory `index.html` cache; config-file proxy uses epoll loop — [2026-05-21-httpd-m1-static-recv-continue.md](docs/release-notes/2026-05-21-httpd-m1-static-recv-continue.md).
- **Compiler E0360 extern ptr ABI:** `verify_mir_extern_abi` before LLVM emit; `li-tests/runtime/argv_ptr_abi.li` — [2026-05-21-extern-ptr-abi-guard.md](docs/release-notes/2026-05-21-extern-ptr-abi-guard.md).
- **HTTP proxy epoll + seam + ptr codegen:** `httpd_li_proxy_*_epoll_i` flushes `proxy_rbuf` on client `EPOLLOUT`; new `std/runtime/seam.li`; `lic` stores full-width `ptr` from `CallExtern` (fixes argv segfault / `verify_fail_li:/`) — [2026-05-21-httpd-proxy-epoll-fix.md](docs/release-notes/2026-05-21-httpd-proxy-epoll-fix.md).
- **HTTP epoll + proxy/LB benches:** land `li_rt_net.c` epoll server, `li-net-httpd` proxy argv routing, snap race fix (reverts broken wave-8 proxy header relay) — [2026-05-22-httpd-proxy-bench-fix.md](docs/release-notes/2026-05-22-httpd-proxy-bench-fix.md).
- **2f AutoVC:** Recursive `decreases`/`requires` call-site VCs typecheck in Lean; parallel-for obligations use `_parN` suffix; `f64` ensures use `Float`; Lean keyword params escaped (`by_`); Linux link skips `-fopenmp` without `omp.h` — `docs/release-notes/2026-05-21-autovc-open-phases.md`.
- **PH-7e:** Loop-based `ArrayMatMul2DF64` (large tiles); `FmaFloatF64` + 16× horner while unroll; tier-1 `matmul_naive` / `horner_pure_li` ≤1.2× C++ (`check-tier1-li-vs-cpp.sh`).
- **G-lean default:** `lic build` runs lake + AutoVC typecheck when `lake` is installed (`--no-lean-verify` to skip); see `docs/release-notes/2026-05-21-glean-default-lean-2i-7e.md`.
- **2i:** `linalg_mat2_at2_float_closed.li` — full 2×2 `@` as `Li.Discharge.mat2_at2_float_spec`; loop-dot closed via static witness + `dot4_int_loop_eval_spec`.
- **2i-b / 7e / 2f slice:** prelude `axpy`, array `**`, reductions; `dot()` VC witness; 2D matrix **CallProc**; `linalg_mat2_callproc_float_closed`; `lic build --strict-lean`; IKJ matmul + release `-ffp-contract=fast`; see `docs/release-notes/2026-05-21-2i-7e-2f-math-surface.md`.
- **Gap closure (2f/2i-b/7d-c/H):** loop-dot VC witness, prelude `norm`, AST parallel race policy, httpd routing contract; see `docs/release-notes/2026-05-21-gap-closure-order.md`.
- **Doc:** master-plan tracker + [provability-gaps](docs/verification/provability-gaps.md) **Still open** section synced to `main` (#151, #148, #150); see `docs/release-notes/2026-05-21-master-plan-gaps-sync.md`.
- **P-linalg proofs (2f partial):** `contracts_verify/linalg_*` — closed int dot/sum/matmul-entry VCs + open loop-dot specimen; `discharge_linalg_int_lean.sh`; see `docs/release-notes/2026-05-20-p-linalg-proofs.md`.
- **Phase 7d-c (partial):** `@vectorized` on `for` — scoped `ArraySimdScope` overrides `@no_vectorize` in loop body; see `docs/release-notes/2026-05-21-7dc-vectorized-for-scope.md`.
- **CallProc array params:** `array[N, T]` and object array fields pass by pointer; see `docs/release-notes/2026-05-21-callproc-array-params.md`.
- **Phase 7d-b (partial):** `@vectorized(lanes=4)` policy + `@no_vectorize` disables array SIMD codegen; `@vectorized` on `for` parses; see `docs/release-notes/2026-05-21-7db-vectorized-codegen.md`.
- **Phase 7e-e (partial):** Safe `f64x4` gather/scatter for `ArrayBinOpF64`; see `docs/release-notes/2026-05-21-7ee-array-binop-simd.md`.
- **Phase 7e-b (partial):** Tier 1 `matmul_blocked` pure-Li IKJ tiles (`li_pure=True`); see `docs/release-notes/2026-05-21-7eb-matmul-blocked-pure-li.md`.
- **Phase 7e-d (partial):** Safe `f64x4` gather codegen for `ArrayDotF64` (insertelement, no vector load from alloca); see `docs/release-notes/2026-05-21-7ed-simd-dot-codegen.md`.
- **Phase 2i-a:** Element-wise `+ - * /` on matching 1d arrays; `sum(a * b)`; see `docs/release-notes/2026-05-21-2ia-array-elementwise.md`.
- **Phase 7e-c (partial):** Math-first HPC docs (`docs/guide/math-hpc-examples.md`, gallery/README/fast-math refresh); see `docs/release-notes/2026-05-21-7ec-math-hpc-docs.md`.
- **Phase 7e-a (partial):** `dot(a,b)` prelude + `simd_dot` pure-Li `a @ b` bench (no `__li_simd_*`); see `docs/release-notes/2026-05-21-7ea-simd-dot-math.md`.
- **Phase 7e-b (partial):** Tier 1 `matmul_naive` pure-Li `@` bench (`li_pure=True`); see `docs/release-notes/2026-05-21-7eb-matmul-pure-li.md`.
- **Phase 2i-c:** 2D `array[M, array[K, float]] @` — shape check, MIR `ArrayMatMul2DF64`, nested index load/store; `li-tests/math_linalg/matmul_*.li`; see `docs/release-notes/2026-05-21-oop-2i-matrix-matmul.md`.
- **Phase 2j-f:** Method call-site `requires` (E0304) + AutoVC for `obj.method()` → `Type_method`; see `docs/release-notes/2026-05-21-oop-2jf-method-vcs.md`.
- **Phase 2j-e:** `type Hash = trait` + `def f[T: Hash]` bounds; static trait impl via `Type_method` procs; see `docs/release-notes/2026-05-21-oop-2je-traits.md`.
- **Phase 2j-d:** `type Derived = object of Base` — flattened layout, static subtyping, `@override` signature checks; `inheritance_*.li` / `override_mismatch.li`; see `docs/release-notes/2026-05-21-oop-2jd-inheritance.md`.
- **Phase 2j-b/c:** `private def` (not exported on `import`); MIR **in-out write-back** for `var` object receivers (`lower_callproc_with_optional_inout`); **7d-c** parallel disjoint via AST `check_module_policies`; see `docs/release-notes/2026-05-21-oop-2jb-2jc-7dc.md`.
- **Cursor rule:** `.cursor/rules/li-test-driven-validation.mdc` — merge review premise (pass/fail `li-tests` prove capabilities).
- **Phase 2j-a:** `obj.method(args)` → `Type_method(self, …)` — parser, typecheck, MIR; `li-tests/encapsulation/def_method_*.li`; see `docs/release-notes/2026-05-20-oop-2ja-method-calls.md`.
- **Phase 2j OOP roadmap** — methods/`self`, traits, inheritance, write-back — `docs/superpowers/plans/2026-05-20-li-oop-roadmap.md`.
- **Language naming conventions** — PascalCase `ClassName` for `type` / `object`; snake_case for `def`, variables, fields; see `docs/language/naming-conventions.md`.
- **Phase 7d-c (partial):** proof builtins `disjoint_elem`, `disjoint_row`, `disjoint_slice`, `row_ok` in typecheck + prelude reserve; see `docs/release-notes/2026-05-20-disjoint-builtins-and-codegen-fixes.md`.

### Fixed

- **CI `test-auth-bearer`:** `build-li-httpd.sh` links `main.li` so `li-httpd` runs `httpd_run_from_argv` (was stub `main` returning 0) — [2026-05-25-ci-test-auth-bearer-main-li.md](docs/release-notes/2026-05-25-ci-test-auth-bearer-main-li.md).

- **G-lean (2f):** AutoVC emits `LiArray α n` (not Lean builtin `Array`); `docs/semantics/Core.lean` + `lake build AutoVC` in CI/`lean-verify-stub.sh`; see `docs/release-notes/2026-05-21-glean-liarray-lake.md`.
- **`CallProc` codegen:** no store on `-> unit` calls; float literal args + f32/i32 coercion; generic return **E0202**; `str`→`ptr` for bootstrap `strcmp`.
- **Composable physics smoke:** `import_physics_runtime.li` **verify_ok** with stmt-call integrate + post-step `pz` (MIR write-back, exit 0).

### Added

- **`httpd_serve_routed_once`** — M1 one-shot accept + `match_route` for `GET /health` (oracle; parallel with httpd-m1-impl/perf PRs); see `docs/release-notes/2026-05-20-httpd-serve-routed-once.md`.

- **`lic httpd validate-config`** — **E0501–E0504** for io/route key/traversal/overlap; `httpd_serve_once` + `route_key_valid`; see `docs/release-notes/2026-05-20-httpd-validate-serve.md`.

- **`lic httpd explain-config`** — desugar `[routes]` to canonical form; golden `check-httpd-explain-config.sh` (C vs Python); see `docs/release-notes/2026-05-20-httpd-explain-config-cli.md`.

- **Phase H M1:** TOML `[routes]` loader — `load_routes_from_toml`, `match_route`, `load_routes_from_routing_fixture` in `packages/li-http`; `runtime/li_rt_httpd.c`; `li-tests/routing/match_routes_toml.li`; see `docs/release-notes/2026-05-20-httpd-toml-route-loader.md`.

### Changed

- Docs: post-PR **#83** sync — [proof-corpus-roadmap](docs/verification/proof-corpus-roadmap.md) run results (16/16 `contracts_verify`); [httpd-prerequisites](docs/ecosystem/httpd-prerequisites.md) P0-lean partial; master plan + httpd plan tables; see [2026-05-20-post-83-docs-sync](docs/release-notes/2026-05-20-post-83-docs-sync.md).

### Added

- Call-site callee **`requires`**: VCs for all resolved callees (incl. **`extern`** + imports); **E0304** with plain-language precondition text when provably false; const-local discharge (`var y = 5`); **`lic build` fails on open `AutoVC`** unless `LI_ALLOW_OPEN_VC=1`; see `docs/release-notes/2026-05-20-call-site-requires-full-gate.md`.
- **Refinement types** at calls and `var` inits: `{x: int | …}` / aliases (e.g. `NonNeg`); **E0305** when provably outside the bound; **`if n >= 0`** branch discharge; call-site Lean VCs; see `docs/language/refinement-types.md` and `docs/release-notes/2026-05-20-refinement-call-check.md`.
- **2f (branch):** `LI_BUILD_VERIFY_LEAN=1` runs `lean-verify-stub.sh` after build; CI uses `LI_BUILD_VERIFY_LEAN_STRICT=1`.
- **Phase H M1 (branch):** `match_route_fixture` in `packages/li-http`; routing tests + `check-httpd-route-fixture.sh`; see `docs/release-notes/2026-05-20-phase-h-m1-routing-match.md`.
- **`packages/li-http`** workspace package (`import http`) — `parse_request` + GET method-line probe via `li_rt_str_byte_at`.
- Phase H P0 runtime: `bytes_len`/`bytes_slice` in `runtime/li_rt.c`, stub `tcp_*` in `runtime/li_rt_net.c`, `li_rt_str_byte_at` for bounded ASCII inspection.
- Fixed-width scalars: `float4`–`float512`, `int4`–`int512` (and aliases); width mismatch is a type error; see `docs/language/scalar-precision.md`.
- Literal suffixes: `3.14f32`, `42i32`, `42u`, `255u8`; binary type + `0b…` literals; `std/binary/binary.li`.
- Documentation: [docs/language/scalar-precision.md](docs/language/scalar-precision.md) (canonical), `packages/li-physics-core/docs/scalar-precision.md`, `std/binary/README.md`; mkdocs + handbook nav.
- `physics.core`: `ScalarPrecision` (`weights_encoding` for binary weights) and profile bit-width metadata (not org-enforced).

### Changed

- Docs: [docs/compiler/llvm-abi.md](docs/compiler/llvm-abi.md) — MIR → LLVM → clang link map; `str`/`bytes` as `i8*`; `extern` checklist; cross-link from [build-pipeline.md](docs/compiler/build-pipeline.md).
- **Breaking:** **E0303** — `ensures true` is rejected on non-`unit` return types (non-`extern`); see `docs/release-notes/2026-05-19-enforce-strict-ensures.md`.
- Composable imports: workspace `packages/*` (via `import_name` in `li.toml`) resolve before `std/` facades (e.g. `physics.rigid`).
- Docs: `composable-by-default.md`, `import-style.md`, `li-net-httpd` README — `def` + `import net.httpd` (not `li_httpd`).
- Physics docs use monorepo package paths (`li-physics-*`, `import physics.*`); philosophy example uses `def`.
- Composable `import_physics_runtime.li` integrates `physics.rigid` semi-implicit step; `rigid_integrate_semi_implicit` takes `b: var RigidBody`.
- `packages/li-net-httpd`: path deps on `li-http` + `li-net`; `httpd_serve` calls `tcp_listen` (stub).

### Fixed

- **CI `test-auth-bearer`:** `build-li-httpd.sh` links `main.li` so `li-httpd` runs `httpd_run_from_argv` (was stub `main` returning 0) — [2026-05-25-ci-test-auth-bearer-main-li.md](docs/release-notes/2026-05-25-ci-test-auth-bearer-main-li.md).

- **`horner_pure_li` harness honesty** — `li_rt_volatile_sink_f64` prevents LLVM from deleting the pure-Li Horner loop; tier-1 verify rejects pure_li timings < 0.45× native (DCE guard). See `docs/numerics/bench-improver-horner-2026-05-20.md`.
- Proof witnesses: `return callee(lit)` / `return callee(ident)` / multi-return procedures discharge `ensures`; call-site refinement VCs use `collect_caller_proof_facts` (const locals + `if n >= 0`); stdlib coverage harness uses `LI_ALLOW_OPEN_VC=1`.
- Codegen: two-pass LLVM emit so `CallProc` to later MIR functions (e.g. imported `match_route_fixture`) is not dropped; `StringLit` / `bytes` call args use `i8*`; `li-tests/routing/match_routes.li` binary exits 0 (`run_httpd_config.sh`); `import_http_lib` / `parse_request_smoke` compile.
- `std_module_to_path`: single-segment `std.bytes` / `std.csv` now resolve to `std/<name>/<name>.li` (was `std/<name>.li`, breaking `import std.bytes`).
- `import_resolve`: parse full workspace `members = [...]` TOML array; absolute importer paths; error on unresolved imports.
- `li-tests/encapsulation/import_parse.li`: local `import_fixture` module (strict import resolve; replaces placeholder `std_math`).
- MIR: `object` field access (`a.x`), field assignment, `var` allocation for multi-field objects, and expanded `CallProc` / parameter lists for object-typed arguments (`compiler/mir/lower.cpp`); regression `li-tests/objects/object_field_smoke.li`.
- MIR: `var dst: Obj = src` copies flattened object slots when `src` is an identifier of the same object type (`emit_copy_object_slots_r`); regression `li-tests/objects/object_copy_init.li`.
- MIR + codegen: object-typed procedure returns use LLVM **struct** returns (`ReturnObject`, `MirFn::return_object_layout`); `CallProc` unpacks struct into `__li_o___cr*` temp slots; `var w: T = foo()` when `foo` returns `T` (`li-tests/objects/object_return_call.li`). Implicit fall-through returns for object procedures return a zero-valued struct.
- MIR: whole-object assignment `dst = src` and `dst = foo()` when `dst` is an `object`-typed local/param and `src`/`foo` match (`collect_object_local_types`, `emit_copy_object_slots_r`); regression `li-tests/objects/object_whole_assign.li`.
- MIR: `emit_copy_object_slots_r` copies fixed `array[N, int]` / `array[N, float]` fields element-wise; nested object array slots register in `g_arr_ctx` for index/assign; `Index` / array `Assign` accept `FieldAccess` array bases; regression `li-tests/objects/object_array_field_copy.li`.
- MIR + codegen: `return_object_layout` / LLVM struct returns include fixed `array[N, int|float]` as `[N x T]` members; `ReturnObject` / `CallProc` unpack and expanded params use aggregates; regressions `li-tests/objects/object_array_return_call.li`, `li-tests/objects/object_mixed_scalar_array_return.li`, `li-tests/objects/object_mixed_param_pass.li`.
- Workspace import: `parse_workspace_members` no longer treats `[workspace]` as the `members` array — `import physics.rigid` loads `packages/li-physics-rigid` instead of the std facade stub.
- Parser: multiline `def` parameter lists (indent after `(` / between parameters); bare `return` for `-> unit` procs.
- Windows CI discovers `LLVM_DIR` via `llvm-config` or `find` when Chocolatey layout differs.
- `packages/li-math-numerics`: remove duplicate `extern proc` contract clauses.
- `packages/li-physics-runtime`: `substep_inv` field and `var PhysicsWorld` step APIs (typecheck; codegen crash on full lib build is a known follow-up).

### Changed

- **Breaking:** Li procedure declarations must use `def`; bare `proc` is rejected (keep `extern proc` for FFI). See `docs/release-notes/2026-05-19-enforce-def-syntax.md`.
- Removed agent/history header comments from `li-tests/`, `packages/*/src/`, `std/` facades, and package scaffold template (kept CWE labels in `li-tests/cve_patterns/`).
- `std/` facades use `def`; composable `import physics.relativity` test calls `physics_relativity_std_tag()`.
- Package mirror CI runs `scripts/check-li-def-syntax.sh`; org mirrors `li-std-core`, `li-std-math`, `li-httpd`, `li-net`, `li-demo` have open sync PRs.

### Added

- Agent-first JSON diagnostics: `lic check --format=json`, `lic diagnose` (`docs/schemas/diagnostic-v1.json`)
- LLM-first design research stub, agent handover comparison, `li-agent-manifest.toml`
- `scripts/lic-fix-suggest.sh`, `scripts/gen-li-agent-manifest.sh`, `li-tests/tooling/diagnose_json_smoke.sh`
- Cursor rule `li-llm-first.mdc`, skill `agent-diagnose-fix-li`

## [0.1.0] - 2026-05-14

### Added

- C++ `lic` compiler skeleton: lexer, parser, typechecker, MIR, LLVM codegen
- Mandatory contracts gate (`requires` / `ensures` / `decreases`)
- `li-tests` manifest harness (47 cases)
- Tier-0 benchmark verify + MD stability stress suite
- Cross-language physics benchmark harness (shared C kernels)
- MkDocs documentation site and CI/local-ci tooling

[0.1.0]: https://github.com/li-langverse/lic/releases/tag/v0.1.0
