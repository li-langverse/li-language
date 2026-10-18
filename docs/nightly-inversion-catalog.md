# Nightly MIR parity: known accept/reject inversion class

Status after seven checker-class ports (post-b424319c walker: index bound,
extern-call effects, stdlib-seal + contract shapes, weak-ensures E0303 and
import-space names, call-site requires E0304, import-visibility E0202). The
nightly sweep compares the self-hosted Li MIR
walker (`build/li-mir-walker`) against the C++ host (`build/compiler/lic/lic`)
over every parseable `.li` file. Byte-exact MIR dump parity is the contract;
the tables below pin the remaining accept/reject boundary.

## Census over all `.li` files (mir subcommand, rc only)

Updated after the 2026-10-16 session (see log below): 537 corpus files
(full-tree `find . -name '*.li'`, wider than the earlier 474-file corpus).

| rc pair                    | files | meaning |
|----------------------------|------:|---------|
| verdicts agree (incl. agreed rejects) |   510 | both-accept and both-reject classes, dump parity holds |
| C++ reject, walker accept  |    26 | inversion class (below) — was 79 pre-port, 41 pre-session |
| walker reject, C++ accept  |     1 | `nested_field_write.li` only — cross-stage reject both compilers
|                           |       | make at their MIR stages (`lic check` cannot see it); both
|                           |       | parity scripts SKIP-list it explicitly, so it is not a false
|                           |       | reject. Raw-rc census red-cell is otherwise empty.

The (near-)empty walker-reject/C++-accept cell is the important invariant:
porter schema changes must never reject what the C++ host accepts. It
re-verified at 0 unaccounted false rejects across a full rc census after each
port — including after the first E0303 attempt, which over-fired on imported
modules (import-httpd files) and was gated to main-module procs before
landing. One operational caveat recorded this session: a full data volume
(100%) silently scrambles rc capture in census loops — `/tmp` writes fail
without aborting the loop, so the cell contents looked flaky
(`nested_index_read`/`import_render_wgpu_fps` one run, `nested_field_write`/
`simd_type_ok` the next). The stable truth was two files, of which exactly one
(`simd_type_ok.li`) was a real regression, fixed below; the other is the
documented cross-stage SKIP.

## Checker-class port log

- Flipped to MATCH this session: 38 census entries (79 → 41 inversion,
  0 false rejects re-verified after each step).

### 2026-10-18: unknown-type type-space gate (tenth class)

C++ rule (compiler/types/typecheck.cpp, driven by `typecheck_module`):
only the main module's own proc heads are typechecked
(`check_count = min(main_proc_count, module.procs.size())`), and every
type annotation they reach must resolve against the merged alias space
(`module.types` — `merge_module` has no type filter, so in-file plus
every transitively imported module's heads) ∪ the C++ primitive set
(`int/bool/ptr/float/float64/f64/float32/f32/float8/f8/int64/i64/long/
int32/i32/uint8/u8/binary/str/unit`; lowercase `bytes`/`string` are NOT
primitives, only aliased via imports). An unknown kind-4 name anywhere
in a checked annotation rejects with `unknown type '<name>'`. Per-proc
binders come from the head's own `[T, ...]` type-var region (cleared
each proc); trait/object inner defs never reach this pass — they live
in `module.types`, not `module.procs`.

Walker mirror (bootstrap/lic/main.li `mir_type_space_check`, wired into
the `mir` command battery between the import-cycle and fx gates):

- Merged alias space (`A_src/A_s/A_e/A_cnt`): file-wide `type`/`enum`
  head scan, then a transitive import DFS in the
  `mir_import_cycle_check` style (clobber + restore of the shared
  stream) so every resolved import contributes its type heads;
  resolution failure latches bail and under-rejects (parse-shape
diff must never fire the gate).
- Annotation scan over the restored main-module stream: def heads
  (`ts_def_head` — type-var capture, top-level param-segment split,
  param + `->` return annotations, `extern` suppression via the
  preceding token) and `var`/`let` decl annotations.
- `ts_type_expr` is a verdict-only mirror of `mir_type_info`'s type
  consumption: `array[N, T]` / `simd[N?, T]` / `list|dict|tuple[...]`
  (via `ts_list_args`, incl. the `tuple[name: type, ...]` field shape)
  resolve scheme-first, then the alias space + type vars by SOURCE-SPAN
  comparison (`fx_span_eq`, never token indexes).

Defects caught and fixed during the port:

- **Scan starved itself**: the paired-if diamond advanced `i` only on
  kind-10/20/21, so any other token (`return`, newline) spun the loop
  forever — probe_parse.li-style files hung. The diamond now advances
  every token (`handled` guard + unconditional fallback).
- **Pos-refinement params are by-value (use after move)**:
  `peek_tk(ts, tok)` marks `tok` moved (C++ borrowck); existing code
  dodges with `tok + 0` — `ts_name_resolves` now does the same.
- **Builtin-collection order**: the collection shape check must run
  BEFORE the alias lookup; as first written `array[N, T]` annotations
  were misjudged as unknown names (four probe files rejected that the
  host accepts).
- **Trait/entity bodies**: `type Hash = trait` containing
  `def hash(self: Self)` must not be typechecked (`Self` has no
  module-level resolution) — `ts_alias_skip` consumes the alias
  head + indented body, counting 2/3 indent/dedent pairs so the stop
  lands on the body-closing dedent. Caught by the full sweep: the
  trait_hash_impl/trait_missing_impl pair flipped MATCH → CPP-ONLY
  before the skip landed.
- **Callable keyword kind**: `Callable` lexes as keyword 48, so the
  ident-only dispatch under-rejected; the dispatch keys on the token
  TEXT, covering both spellings.

No-false-reject probes: poison `i: NoAlias` annotation rejects on BOTH
lanes (walker 200-code = def-param fire); imported-alias annotations
(`Reader`/`StringView` via `import std.bytes`) accepted; refinement
alias, `array[4, int]` decl and generic `[H: Hash]` heads all rc=0.

Full sweep (474 files incl. proof-db): match=359, known-diff=0,
real-gap=0, cpp-only=10, li-only=0, neither=105. The 10 CPP-ONLY
entries are exactly the pre-catalogued baseline-SKIP residue files
(no CPP-ONLY delta); the gate-on vs gate-off sweep buckets are
byte-identical — every census unknown-type shape was already
both-lane via earlier classes, so this port co-fires without changing
any census verdict (its class is exercised by the poison probes and
holds the gate for future corpus entries). The class is now LOCKED:
li-tests/typecheck/{unknown_type_param,unknown_type_local}.li are in
check_li_mir_parity.sh's REJECT_CORPUS (8/8 both-reject), and
check_li_mir_parity.sh's byte-exact corpus is the accept-side control
(trait pair locks the alias-body skip, callable_param.ci-like shapes
type the Callable dispatch, composable imports type imported heads).

Gate-run defect (check-lane battery, fx import-space residue):
`effects_check`'s first proc-collection loop had lost its
`fx_skip_typeparams` + `expect_k(49)` + `decl/heap[0] = 0` resets in
the fx-port edit (the second loop kept them) — pi stopped on `(` and
the unknown-head error fired on 114 corpus files. Restored the four
lines (mask_row stays -1: shared callers stay inert).
check_li_check_parity moved 44/158 → 158/158 with 0 false rejects.

### 2026-10-17: contract-shape rules (ninth class)

Three same-day rules, one pass, all main-module-only in C++:

- **E0303 literal-return postcondition** (typecheck.cpp
  `postcondition_is_false_for_literal_return`): for a value-returning
  non-extern proc, if the body contains EXACTLY one `return` anywhere
  (`count_returns == 1`) whose expr is a number literal, and any ensures
  clause is a single comparison with `result` on either side and an int
  literal on the other, the comparison must hold — else E0303. Walker mirror:
  `fx_mir_pass1`'s contract loop captures every `result <op> <int-lit>` /
  `<int-lit> <op> result` ensure arm (rq_capture_clause's exact token shape,
  keyed on `result` kw 46), `parse_stmt`'s return branch counts ALL returns
  while a counting mode is on (preg 17.0, set around pass1's def-body parse)
  and folds the single expr-bearing return's literal (respecting the `-`
  fold: kinds 60+5, exact pi-delta check); the post-body eval compares via
  the existing `rq_op_violated` matrix (op kinds 66-71) and fires E0303
  through the established err channel.
- **Extern-shape policy substring** (policy.cpp: source
  `.find("extern def str" + "cpy")` on the raw source): mirrored with
  `li_rt_str_contains2` — a runtime two-piece needle — because the C++ rule
  fires on the substring's presence itself, so a contiguous copy anywhere in
  a checked file (comment, string, or the walker's own main.li) rejects it;
  runtime addition, li_rt.c/h.
- **Heap-type raises-Alloc** (borrowck.cpp effects_check_module:
  `proc_mentions_heap && !has_effect(raises, Alloc)`): per-proc pass2 walk
  already tracks heap-typed params/return (`heap[0]`, fx_is_heap incl.
  str/bytes superset) and `decl[0]` carries the raised-effect bits (Alloc =
  bit 2); the VarDecl scan needed a new watch — preg 17.4 (watch on) /
  17.3 (heap seen) — hooked in BOTH consumers (parse_stmt's and
  fx_mir_top_stmt's var-decl branches); verdict slotted into pass2's
  per-proc verdict zone next to the extern-IO rule.

Defects caught during the port: the return-count hook was planned but never
landed in the first edit round (probes 903/904 + E0303 marker found eval
entry with count 0 — re-inserted into parse_stmt's return branch); a
mid-flight second `accept_k` call risk in the previous port class was audited
and avoided again here; a bodyless def could inherit the previous proc's
heap bit — cleared at each pass2 proc entry. ONE transient build break:
placing the full policy needle in main.li made the *walker's own source*
pass the substring test during bootstrap (the C++ policy fires on any file
containing it) — hence the runtime-side needle assembly.

Probes: all three fixtures 1/1; value-returning accepted siblings stay 0/0.
3 census entries flipped to agreed-reject (inversion 29 → 26, 0 unaccounted
false rejects re-verified; nightly 358 MATCH / 0 DIFF; check parity
158/158). Recorded limitation: float-literal arms (kind 7) are not captured
(the rq_lit_val float path is approximate) — conservative under-reject;
no current census file needs it.

### 2026-10-17: parser-shape rejects (eighth class)

C++ rule (compiler/parser/parser.cpp, pre-typecheck): three parse-shape
rejections, all through `expect(...)` — `if`/`elif`/`else` statements miss
ing their `':'` colon after the condition ("expected ':'"), an empty expression
after a BinOp in a contract line ("expected expression" from parse_primary
at the newline), and a non-extern `def` whose contract run is followed by any
thing other than the `=` body marker ("expected '").

Walker mirror (bootstrap/lic/main.li): the walker's quiet parse machinery
already detects all three shapes (`expect_k` latches err[0] on a missing
colon; parse_primary's `err[0]=1; return 0` fires on the empty contract RHS;
a def ending the stream before `=` returns 0 from accept_k) but the fx lane
swallows those signals as benign scratch. The port latches a dedicated verdict
slot — preg 22 (free: 14-22 unused; 20 = import failsafe, 23 = E0304 fire) —
at exactly the three consumption-shape sites, and `fx_mir_effects` clears it
before each pass-1 walk, reads it into `found[0]` after, so every latch is a
verdict from THIS module's pass1 + the import-merge consumption (matching the
C++ `load_module_recursive` diagnostics propagation). err[0] flows untouched:
each hook snapshots it, clears it around the parse call, and restores it so
the fx pipeline's own channel keeps its established behavior.

Probes: build_std_csv (colon), parser_reject_bad_expr (empty RHS,
parser_reject_no_body (missing `=`), game_physics_smoke (colon) reject 1/1;
valid-body files stay accepted. 4 census entries flipped to agreed-reject
(inversion 33 → 29, 0 unaccounted false rejects re-verified after each leg).

### 2026-10-16/17: import-visibility unknown-callee E0202 (seventh class)

C++ rule (compiler/types/import_resolve.cpp `merge_module` — public-only
merge — plus typecheck's `procs.end()` miss): a `private def` from an
imported module is excluded from the caller's proc map, so a call to it is
rejected; own-file decls and imported public rows stay resolvable. Walker
mirror (bootstrap/lic/main.li): `fx_mir_pass1` captures the visibility bit per
row (the caller lane stores the *call-visible* bit — own-file rows are always
callable regardless of visibility, matching the unfiltered ctx.procs own
half), `fx_import_merge` carries and maintains the bit through the transitive
merge (`tvis`), and the unknown-callee gate in `rq_check_call` resolves a
callee against the merged rows (`iem_*`, `iem_uq`-gated) — unknown names
latch E0202 through the preg-23 fire channel. Two fail-safes so the gate can
never invent a reject: preg 20 (any unresolved import — C++ resolves it fine)
disables the gate entirely, and the 256-row table cap sets the same latch
when the own-row sweep binds at capacity.

Probes: `private_method_use.li` (cross-file `vault_secret` call) rejects
1/1 with E0202 in both lanes; same-file/same-visibility calls, field-level
private access (`private_field_access`, `leak_imported_vault`), and the
composable/async import family (`import_http_lib` et al.) stay accepted 0/0.
8 census entries flipped to agreed-reject (inversion 41 → 33, 0 unaccounted
false rejects re-verified).

Three defects found and fixed during the port:

- **Transposed merge-call args (both lanes)**: callers passed the 1-cell
  counter array in the `tvis`(256) slot and the 256-row array in `tcnt`(1) —
  the merge loop's `tvis[idx1] = 1` then wrote far past the caller's counter.
  Symptom: SIGBUS `EXC_ARM_DA_ALIGN at 0x100000001` — two adjacent flag cells
  (1,1) composed as one pointer. Fixed in both `fx_mir_effects` and
  `effects_check` call sites. (A stale `build/li-mir-walker` binary made this
  look flaky at first — the walker and `lic-from-li` are the same program, so
  refreshing both after every build is mandatory.)
- **Unknown-callee over-fire on prelude SIMD intrinsics**: the gate fired
  E0202 on `__li_simd_splat_f64`/`_mul_f64`/`_add_f64` and
  `__li_horiz_sum_f64`, which C++ resolves by direct name in check_expr
  BEFORE the procs lookup (compiler/types/typecheck.cpp ~L811), flipping
  `simd_type_ok.li` MATCH→SKIP in `.mir_parity_baseline`. Fixed with an exact
  four-name exemption in the same gate — deliberately NOT extending the
  `seal_prelude_proc` reserved-name roster, so the stdlib-seal class (fifth
  entry) and check-seal-name-sync stay untouched.
- **Census flake, not a port defect** (recorded above): the 100%-full volume
  scrambled rc capture; the stable false-reject pair contained exactly one
  real regression (the SIMD over-fire above) plus the documented
  `nested_field_write` cross-stage skip.

### 2026-10-16: call-site requires gate, E0304 (sixth class)

C++ rule (compiler/types/typecheck.cpp `literal_requires_is_false`, call
sites via `check_expr`): at a call to a proc with an ident-shaped
`requires false` → reject unconditionally; with a single BinOp
`param cmp IntLit` requires and an int-literal argument → reject iff the
comparison fails. Walker mirror (bootstrap/lic/main.li): `rq_capture_clause`
captures shaped requires into the rq_* tables; `rq_check_call` evaluates
them at literal-arg call sites; the verdict rides the dedicated
`li_rt_preg` registry slot 23 (cleared per walk, latched after) so benign
err[0] latches cannot fake a reject; `mir_rq_check` walks every
main-module proc body with the fire slot wired, and the `mir` driver
aborts with `mir error` before the dump when fxr==1.

Three defects caught and fixed during the port:

- **Table 23 did not exist**: `LI_RT_PREG_TABLES` was 14 and the sizes
  initializer had fewer entries than tables, so `li_rt_preg_set(23,...)`
  silently no-oped and the fire never reached the walker's read site.
  Extended to 24 tables with a regenerated initializer (runtime/li_rt.c);
  round-trip proven in a direct C harness (`t23=777`).
- **`requires true` fired E0304**: the ident-shaped capture stored `ap=-1`
  for every bare ident predicate, and `rq_check_call` fired on it
  unconditionally — four real files (seam_policy_ok, li-nanoreactor,
  li-physics-runtime, li-studio lib) rejected. Mirrored the C++ gate:
  only `requires false` captures the ap=-1 entry.
- **`rq_lit_val` read kinds from ts (byte-span array) instead of tk**: a
  start offset is never 5/7/60, so every captured compared as 0 →
  `param < lit` / `<=` / `==` fired for everything, `>=` / `>` / `!=`
  never. With the correct kinds the six-op verdict matrix is exact in
  both directions (param-left and literal-left, incl. signed literals).
Also mirrored two C++ shapes: compound right operands (`param op 0 - 5`)
are not captured (C++ parses a single BinOp; terminator check after the
literal), and the capture only fires when the comparison operand is the
parameter position (probe `-5 < port` vs `-6` confirmed both signs).

Probes: `callee(-1)` vs `requires x >= 0` rejects with E0304 in BOTH
lanes (mir + check); the caller_requires_fail/ok/local_ok triple now
agrees 1/0/0 with the C++ host; clean probes stay rc=0. The earlier
32-file over-reject class (multiline_decl) is gone.

### 2026-10-16: stdlib-seal family (fifth class)

All five stdlib_seal entries closed by mirroring `check_source`'s seal
walk (bootstrap/lic/main.li `check_source` branches for declarative heads)
into the mir lane as `fx_seal_check`, invoked from `fx_mir_pass1` right
after pass 1. Same collections as the check lane (`seal_def_check` for
duplicate definitions E0330/dup-E, `seal_check_reserved` for stdlib-name
shadowing including the `std\` path exemption identical to the reference
line). Mechanism closure: duplicate_proc, shadow_list_type, shadow_print,
shadow_print_dep, shadow_std_symbol — all five now 1/1 in both lanes;
ocontrols (ok_control, shadow_print_via_import) stay 0/0.

One defect caught by the gate: the seal walk advanced `pi` past the `type`
keyword before calling `fx_skip_typealias`, which expects to start at the
keyword — `type Limits = object` mis-consumed, set err[0] deep in the
skipper, and the `fired` latch misread it as a seal violation, codelessly
rejecting object_field_smoke.li (a previously matching file). Fixed by
peeking the alias name at pi+1 without moving pi, and clearing err[0] at
each walk head so only seal calls can latch `fired`.

### 2026-10-16: weak-ensures E0303 + import-space procurement (third + fourth class)

Two same-day ports, both landing from the shared `fx_mir_pass1` collection
pass so the mir and check lanes move together:

- **Import-space proc names** (bootstrap/lic/main.li `fx_import_merge`,
  factored out of `fx_mir_effects`; `effects_check` and `fx_mir_pass1` share
  the span-capture triple tables): every transitively imported module's proc
  names/extern flags/raises bits enter the lane tables, so `fx_proc_ext` /
  `fx_proc_eff` resolve imported names (`fx_span_eq` cross-buffer compares)
  and the builtin-IO exclusion applies C++'s `proc_map.count(ident) == 0`
  gate. This closes the recorded `shadow_print_via_import.li` check-lane
  divergence — CORPUS_KNOWN_DIVERGENCE is now empty (the empty-array
  iteration got the bash `set -u` idiom).
- **E0303 weak-ensures** (`fx_mir_pass1`, gated `vc_on` for main-module
  calls only, non-extern only, `-> unit` exempt): a bare `ensures true` on a
  value-returning proc is a single-token predicate ended by the clause
  terminator; compound predicates never match (`is_true_literal`). Six
  typecheck-family census entries flipped to MATCH on both lanes          (probe_alias, probe_arrret, bad_numeric_mix, bad_int_float_add,
  bad_array_index); `scalar_width_mix_fail` remains open (numeric scalar
  width mixing needs operand-type inference; the check lane already rejects
  it — only the mir lane lacks the rule, recorded deliberately out of scope
  like the other checker classes).
- Same pass, same gate, same day: the two missing-clause shapes (E0201
  `proc missing requires clause`, E0202 `proc missing ensures clause`,
  ungated in C++ check_proc) plus the `ensures false` arm of E0303
  (is_false_literal). Five more census entries flipped (advisory/check_mixed,
  physics/game_runtime_smoke, prove_reject/missing_contracts,
  security/proof_safety/{ensures_false_rejected,missing_ensures_rejected}).
  One wiring defect caught during the port: the seed's verdict rides
  err[0], which fx_import_merge clears at entry — effects_check now
  captures the seed return and re-applies it after the merge so the check
  driver's err_any sees it.

Load-bearing infra note (not a checker class): the li backend's alloca for
ptr-element arrays beside the effect tables was sized 4 bytes/element while
accesses strode 8 bytes — the `psrc` name-table's tail slots aliased the
span tables, poisoned pointers reached `strlen`. Fixed in
compiler/mir/lower.cpp (VarDecl arrays set `array_is_i64` for
ptr/int64/i64/long elements; not a dumped field, so MIR text parity is
untouched — nightly byte-exact gate re-verified).

- Flipped to MATCH by the E0303-class ports: 33 census entries
  (79 → 46 inversion,
  0 false rejects re-verified after each step).

### 2026-10-15: extern-call raises-IO rule (second class)

C++ rule (compiler/types/borrowck.cpp `proc_mentions_extern_call` +
`effects_check_module`): a non-extern main-module proc is rejected with
`proc calls extern but does not declare raises IO` when a top-level body
statement's root expression is a direct call to a declared extern proc (or a
user-declared extern `print`; `echo` is excluded by name) and the proc does
not list `IO` in `raises`. Root expression means C++ `Stmt::expr`: bare Expr
statements, `return` exprs, and Assign RHS — not var-decl/borrow inits
(these land in `Stmt::init`), not conditions, not nested call arguments, not
nested blocks.

Walker mirror (bootstrap/lic/main.li `fx_mir_effects` + `fx_mir_top_stmt`,
run in mir mode before `mir_module`): a two-pass scan reuses the existing fx
collection machinery (proc names, extern flags, `raises` bits via
`fx_eff_bit`), then flags only the three root shapes at top level and
requiring the IO bit. Non-root call positions consume via the walker's own
quiet parser machinery so token flow stays identical.

- Flipped to both-reject: `volatile_sink_no_io.li`,
  `golden_dot4_ones_twos.li`, `golden_tier1_dot8.li`, `leak_censor.li`.
- Residue (known divergence, not implementation): calls to externs declared
  in OTHER files (imports) are outside the single-file scan because C++
  merges imported procs into its proc_map; no current census file relies on
  it.
- Deliberately out of scope (other C++ checker classes): the print/echo IO
  rule (`io_fail.li`), heap `raises Alloc`, await/async walker-only rules.

### 2026-10-15: static index bound check (first class)

C++ rule (compiler/types/typecheck.cpp): for every `Index` expression on an
array-typed base, an int-literal index must satisfy `0 <= i < array_size`;
otherwise the host rejects with `array index out of range`.

Walker mirror (bootstrap/lic/main.li `mir_lower_expr`, three literal-index
sites): generic fixed-array load, object-field load `o.x[i]`, nested
object-field load `d.tier.vals[i]`. Sizes come from the walker's canonical
tables only — current-proc local/param int/float array tables and object-field
element counts. Unknown sizes (named-type aliases, computed/temp bases) stay
unchecked, so the walker rejects a strict subset of what the C++ host does.

- Flipped to both-reject: `cwe120_stack_buffer_oob.li` (frame[40] on 32),
  `cwe125_negative_index.li` (buf[-1], folded negative literal),
  `board_oob.li` (board[200] on 200).
- Still open: `cwe787_oob_index.li` (`g: Grid` alias; alias RHS
  `array[8, T]` is not resolved to a size by `mir_type_info`, which reports
  named types as class 6 with no element count).
- Store-side literal indices (`arr[i] = v`) are not checked yet; no current
  census file reaches C++ through a store site alone.

## Remaining inversion class: 29 files

The C++ host rejects these during its analyze/typecheck/borrow-verify stages,
which the walker does not implement classes for. The fresh 2026-10-17
classification below groups all 33 files by C++ diagnostic family (message ->
files, `lic check` stderr normalized); the older 79-file breakdown is retired
— its normalization reference overlapped several since-closed classes.

-  8 — parallel-for disjoint/Sync proofs
  (parallel_requires_disjoint; overlapping shared mutable memory; Sync proof;
  borrow-mut-across-iters): race_shared_memory/{false_disjoint_proof,
  mut_capture_no_sync, missing_disjoint_clause, overlap_par_slice,
  false_disjoint_requires_true, shared_mut_write, borrow_mut_across_iters} +
  decorator_exploits/missing_disjoint_at_parallel. Largest open cluster;
  proof-carrying parallel checks, project-level port.
  TRIAGE (per-file, C++ host rejects via types/policy.cpp heuristics +
  types/policy_module.cpp structured decorator checks; walker currently
  ACCEPTS — the safety-critical under-reject direction):
  * Rule A — parallel `requires` must prove disjoint: missing_disjoint_clause
    (no requires), false_disjoint_requires_true (weak `requires true`
    witness), overlap_par_slice (no requires; same generic rule),
    missing_disjoint_at_parallel (@parallel def without disjoint= arg, also
    policy_module.cpp decorator rule). MEDIUM: the walker already parses
    parallel-for requires clauses and lower.cpp sets DEC disjoint_proven
    from has_disjoint — the port is a structured witness check (require a
    disjoint_* call, reject literal `true`), mirroring
    contract_expr_is_disjoint_proof/decorator_disjoint_value_ok.
  * Rule B — overlapping shared mutable memory (constant-slot write):
    shared_mut_write (buf[0] store), false_disjoint_proof (grid[0][0] store
    contradicts a present disjoint_row proof — variant of the same rule).
    EASY: constant-index store token scan inside the parallel body.
  * Rule C — mutable capture requires Sync proof: mut_capture_no_sync
    (counter = counter + 1). MEDIUM: needs to distinguish outer-scope locals
    assigned inside the parallel body from body-local vars.
  * Rule D — borrow mut forbidden across parallel iterations:
    borrow_mut_across_iters. EASY: `borrow mut` token scan inside the
    parallel body.
  Ordering: D, B first (token-scan, 3 files cheap), then A (structured, 4
  files), C last (capture analysis, 1 file).
  CLOSED: mir_policy_check (bootstrap/lic/main.li) mirrors check_source_policies
  as a token-adjacency scan on the lexed stream, wired into the `mir` command
  battery ahead of the fx pass. The host's rules are file-wide predicates run
  on every `lic mir`, so a file it accepts can never trip the scan — full sweep
  confirms zero new CPP-ONLY. All four rules closed in one pass because D+B
  (borrow-mut / constant-slot) and A+C (weak witness / capture) are the same
  file-wide shape in the host. Sweep delta: li-only 20→12, neither 85→93;
  the 8 cluster files reject identically on both lanes. Scope note: the scan
  runs on the `mir` lane only — the `check` battery is not yet mirrored.
-  5 — unknown name in the merged proc/type space: unknown type `bytes`
  (bytes/reader_writer_smoke, std/bytes/bytes.li, packages/li-bytes/src/lib.li),
  unknown method `missing` (encapsulation/def_method_missing), plus unknown
  type `S` inside docs precision-physics-math (which also carries mixing +
  mismatch failures). Needs the walker's type/alias space extended before
  name resolution.
-  4 — borrow/move (borrowck.cpp): mut-borrow misuse (borrow/double_mut) and
  use after move (borrow/use_after_move,
  packages/li-studio smoke studio_agent_invoke, li-gui viewport_selection).
  CLOSED: per-param mask accumulation fix in fx_skip_params (mvis 0/1/2
  enum, preg 15 is-var / preg 16 scalar) keys merged import-space rows
  correctly; C++ host and the bootstrap walker now reject all four files
  identically, and the fixtures are locked into check_li_mir_parity.sh's
  REJECT_CORPUS against regression.
-  4 — type mix/mismatch: generic return type mismatch
  (generics/return_mismatch), variable type mismatch
  (math_linalg/elementwise_len_mismatch, docs precision-physics-math), int
  width mixing (typecheck/scalar_width_mix_fail; its int/float sibling is the
  known open entry from the E0303 port).
-  2 — static index residue from the first port: cwe787_oob_index (alias
  RHS `array[8,T]` unresolved), cwe787_dyn_index (constant/refinement-bound
  rule not ported).
-  2 — @vectorized lanes-only (lanes=4 exempt): vectorized_lanes8_fail,
  decorators_parse.
-  2 — import_cycle pair (a/b detect each other; a single-file scan can
  only reject the first traversal — the walker must mirror the DFS
  cycle-detection path).

Groups land 5+4+2+2+2 = 15 (parser-shape group closed by the eighth port;
contract-shape group closed by the ninth; borrow/move group closedby the fx_skip_params mask fix; race_shared_memory group closed by the
mir_policy_check scan — all locked into the reject corpus or sweep buckets).

## Residue triage (2026-10-17 sweep, li-only = 12)

Per-file verdicts for the 12 surviving LI-ONLY divergences, grouped by the
walker rule each needs. Effort: EASY = token/literal scan, EASY-MEDIUM =
single local rule, MEDIUM = cross-module or recursive machinery.

-  unknown-method member call: def_method_missing (`w.missing()` on
  Widget). MEDIUM: walker needs the object method table + member-call
  validation after parse.
  CLOSED: the mir lane's kind-13 method-call lowerer now rejects when
  the receiver var resolves to a KNOWN object type but
  mir_method_pid(`<Type>_<method>`) returns -1, mirroring C++
  typecheck.cpp's MethodCall rule (known receivers strict, unknown
  receivers lax). The `check` lane already had the twin rule in
  pl_check_postfix. Sweep: li-only 6→5; the fixture rejects on both
  lanes; all object/encapsulation corpus files stay MATCH (zero
  CPP-ONLY delta).
-  std bytes module space: reader_writer_smoke + packages/li-bytes/src/lib.li
  (`bytes`/`Reader`/`Writer`/`StringView`/reader_* unresolved via
  `import std.bytes`). MEDIUM: stdlib module resolve/plumbing in the
  merged proc/type space; 2 files, one machinery.
-  import-cycle DFS: import_cycle_a/b. MEDIUM: recursive import walk with
  an on-stack cycle set; both files reject when either direction cycles.
  CLOSED: mir_import_cycle_check (mir lane, wired into the `mir` command
  battery ahead of the fx pass) mirrors import_resolve.cpp's load DFS
  in-progress set at depth-2 transitivity (a depth-2 mirror rejects both
  files of the pair from either direction, matching the C++ host's cycle
  detection for the corpus's only cyclic shape). Each resolved import is
  token-lexed into a LOCAL scratch stream so the root's token arrays stay
  intact; unresolved imports are skipped without rejecting (resolution
  fail != cycle). Reverse note: the `check` lane already carries the twin
  2-level check in the encapsulation battery. Sweep: li-only 4→2; both
  fixtures reject on both lanes; import-heavy corpus files stay
  lane-equal (zero CPP-ONLY delta, match 359). Depth beyond 2 remains
  uncovered — no corpus shape needs it.
-  generic return resolution: return_mismatch (`return x` with x:T in an
  int-returning generic proc). MEDIUM: T-instantiation check on the
  return path.
  CLOSED: mir_generic_ret_check (mir lane, wired into mir_proc) mirrors
  typecheck.cpp's top-level return rule: a bare `return <ident>` whose
  binder is generic-typed (param `x: T` or local `var y: T`, from the
  header `[T, ...]` region) rejects when the first `->` return type is
  CONCRETE — the host's TypeVar-vs-concrete assignable failure.
  Self-typed `-> T` identity procs stay accepted; top-level depth comes
  from INDENT/DEDENT counting (C++ checks only p.body). First cut
  compared idents against the type-param names and missed the actual
  shape (binder typed T, not named T) — the sweep spot-check caught it
  (cpp=1 li=0) and the fix registers generic-typed binders via the
  param-segment + var-decl scans instead. Sweep: li-only 5→4; fixture
  rejects on both lanes; probe_alias/probe_arrret/fib stay lane-equal
  (zero CPP-ONLY delta).
-  array-shape binop rule: elementwise_len_mismatch (`a * b` with
  array[4]/array[8] elems). EASY-MEDIUM: elem-type + array_size equality
  on the elementwise binop's two operands.
  CLOSED: mir_var_decl now captures the initializer token window and, for
  array decls (ti[0]=5), resolves both operand sizes of any
  ident-op-ident pair via mir_farray_size/mir_iarray_size (same family
  only; s/e args are SOURCE offsets per mir_slice_eq) and rejects when
  the declared ti[2] != max(szl, szr) — exactly the C++ type_of + var
  binding equality of typecheck.cpp (E0302 class). Broadcast len-1 binds
  unchanged (array[4] = a1*b4 accepts; array[8] = a*b accepts). Sweep:
  li-only 10→9; the fixture rejects on both lanes, and all math_linalg
  / vec corpus files stay MATCH (zero CPP-ONLY delta).
-  int-width family: scalar_width_mix_fail (int32 + int64 operand).
  MEDIUM: width derivation for param types + binop operands (float
  sibling documented as the open E0303 entry). EASY once the family
  helper lands.
  CLOSED: mir_width_check (mir lane, wired next to mir_index_check in
  mir_proc) mirrors typecheck.cpp's BinOp width rule: arithmetic
  (+ - * / // % **) over two same-family different-kind scalars reject.
  Classes follow the C++ TyKinds — plain `int` (Int), `int32`/`i32`
  (Int32), `int64`/`i64`/`long` (Int64), `uint8`/`u8` (UInt8) are four
  DISTINCT int kinds with no implicit widening; float likewise (Float
  plain/f64/float64, Float32, Float8); literals class as Int/Float.
  Params register from the token param scan, locals from `var NAME:
  TYPE` decls; unregistered idents (call results, arrays, ptrs) never
  fire. Sweep: li-only 7→6, the fixture rejects on both lanes, and all
  3 fixed-width-scalar corpus files stay lane-equal (zero CPP-ONLY
  delta, match 359).
-  constant-index bounds: cwe787_oob_index (`g[9][0]` on
  array[8, array[8]]). MEDIUM: constant-index bound rule in the
  single-file scan.
-  refinement-bound dynamic index: cwe787_dyn_index (`buf[i]` with
  unrefined i). MEDIUM: carry the refinement-bound rule from the first
  port's residue note.
-  BOTH CLOSED as one mir_index_check pass (mir lane, wired into
  mir_proc over the exact walked body span [pi before mir_stmts, pi
  after]):  (a) constant index must satisfy i < resolved base size
  (array-alias params resolved via a file-wide `type N = array L`
  token scan);  (b) a bare ident index is accepted only when the name is
  a loop var (`for/parallel-for 0..<N`, `while i < N` — C++ locals are
  per-proc flat maps, no scope pop) or a refinement-alias-typed param
  (`type Idx8 = {i | ... i < B}`), with bound <= base size;  candidate
  bases = non-`var` array params + non-`var` array-alias params
  (typecheck.cpp:1114 requires_bounded_index; locals and `var` bases
  stay lax). Sweep: cwe787 pair li-only 9→7; zero CPP-ONLY delta at
  final state (match 359 restored).
  OVER-REJECT lessons (caught by the sweep, fixed in-place):
  * ident matches must be by SOURCE TEXT (mir_slice_eq on ts/te spans),
    never by token index — an event occurrence and an lv creation site
    always have distinct token indexes;
  * the preg-based is_var read was actually the element-family code —
    the param list is rescanned at TOKEN level (`name : [var] type`) with
    bracket-depth-aware segment splitting instead;
  * the dynamic rule must require `[ ... ]` to close immediately after
    the index token: `a[f(x)]` (call as index) is a C++ expr kind the
    prover folds, not an Index-with-Ident it rejects.
-  @vectorized lanes ceiling: vectorized_lanes8_fail (lanes=8).
  EASY: lanes literal check. Same rule also closes decorators_parse
  (stacked decorators + lanes=8), its parse side already partially
  ported. 2 files, one rule.
  CLOSED: mir_policy_check now tracks has_at_vec (Ident `vectorized`
  preceded by @) and the whitespace-independent `lanes` `=` `8` token
  triple, firing the policy.cpp rule on the mir lane. Sweep: li-only
  12→10, neither 93→95; the 2 files reject on both lanes and all six
  decorator ok-files stay accepted (zero CPP-ONLY delta).

Port-order recommendation: lanes ceiling (EASY, 2 files) → array-shape
binop (EASY-MEDIUM, 1) → constant-index bounds → refinement index →
int-width family → member-call validation → std bytes plumbing →
import-cycle DFS → generic return resolution. (All listed classes have
since landed; see the eleventh-class entry below.)

### 2026-10-18: borrow-lane walk mirror (eleventh class) — CPP-ONLY emptied

The 10 primer CPP-ONLY residue files (studio composable / lig cluster:
packages/li-math, li-render scene_md_particles, li-ui studio_palette,
li-studio src/lib.li + studio_command_palette, studio_compose_panels,
studio_vertical_profile_roundtrip, packages/lig custom/lib.li +
lig_device_probe, li-tests/composable/import_studio_compose) all fired E0311
on the walker while the C++ host accepted. Four C++-mirror fixes in the
borrow walk (bootstrap/lic/main.li), driven by an emit-channel debug trace
(bk mark / use-fire source offsets + per-arg detection peeks, later removed):

- **Call-arg move ordering** (probe probe_same_arg_call.li, both-accept
  probe): the walker marked each plain arg the moment its own parse ended;
  C++ checks EVERY arg's uses first (check_expr_uses) and marks once after
  the whole arg list (check_call_moves). The bk call branch now buffers
  per-slot plain flags (arg_toks/arg_plains) and applies the mover marks
  after expect(50), reproducing the C++ order exactly — nested calls keep
  inner-marks-before-outer marks. Slots are zeroed at record time: the
  first cut read stale `arg_plains` bits and wrongly marked number/field
  args in li-studio/src/lib.li (found by offset-bisect + the arg-detect
  trace). Args past the 16-slot buffer are not marked (the pre-port j>=16
  verdicts read beyond the 16-bit bp/preg rows; no corpus shape needs them).
- **Field/MethodCall bases**: C++ check_expr_uses has no Field/MethodCall
  case, so a moved-then-field-read (`x.f` / `x.m(...)`) never use-checks
  its base; only Expr::Ident and Expr::Index recurse. The bare-ident
  branch now suppresses bk_check_use when the next token is the dot (kind
  72) — lig_custom_target_ready's `descriptor.field` after a mover call.
- **UnaryMinus opacity** (kind 34 = non-literal `-x`, kept as a node per
  parser.cpp's mover comment): check_expr_uses has no UnaryMinus case, so
  the subtree is opaque to the host borrow lane; the walker runs the
  operand bo=0 (rq lane stays on, mirroring typecheck.cpp's visit for
  callee-requires).
- **If tails**: check_stmt(If) walks then_body only — the walker now runs
  elif/else blocks bo=0 (the loop-body bo=0 rule already existed).

No-false-reject control: use_after_move/double_mut stay both-reject,
borrow_ok/loop_move_ok stay both-accept, and both new probes are
CORPUS-locked (42 byte-exact files + the 8/8 REJECT_CORPUS, rc=0).

Full sweep (478 files incl. proof-db): match=371, known-diff=0, real-gap=0,
cpp-only=0, li-only=0, neither=107 — the CPP-ONLY bucket is EMPTY for the
first time (371 = 359 + the 10 former CPP-ONLY + the 2 locked probes).
check_li_check_parity 158/158; line hygiene rc=0; nightly rc=0 with the
baseline refreshed (.mir_parity_baseline +4 entries).

## Decision

Implementing the analyze surface inside the bootstrap walker is a
project-level port (borrowck, effect system, proof-carrying parallel
checks, policy walks). Each new reject must provably not flip any of the 358
byte-exact files, so classes are ported one at a time with the full
462-file census as the no-false-reject gate — exactly what the first two
ports satisfied. Until the remaining classes land, those files stay
baseline-SKIP: the nightly rc=0 state is exact, and MIR dump parity across
the both-accept class is complete. Update 2026-10-18: with the eleventh
class's borrow-walk mirror fixes the CPP-ONLY bucket is empty (cpp-only=0,
li-only=0, real-gap=0 at 478 files); the both-accept class is byte-exact and
every reject fixture is corpus-locked.

Note: the nightly script requires bash >= 4 (`declare -A`); on macOS run it
with `/opt/homebrew/bin/bash` (or equivalent), the default /bin/bash is 3.2.

## Update 2026-10-18 (batch 3: args-layout + tuple ellipsis + check-lane privacy)

Four remaining witness inversions closed on the check lane:

- **Multiline call arguments (package/loop_call class)**: the C++ host
  skips layout tokens (newline kinds 1/2/3) inside every parenthesized
  argument list; walker lanes dropped that tolerance unevenly. Every
  walker lane's args loop now skip_newlines before each argument, before
  the `,`/`)` decision and before the closing expect — tc_primary's
  print/procall loops, pl_check_primary/pro_postfix loops, and — the
  silently failing one — bk_check_primary/bk_check_postfix (borrow set
  err with no diagnostic, so the probe batch showed only "check error").
  Probe corpus: ml_params (params only) ok; ml_call/shape variants all
  byte-exact AST + verdict parity.
- **`tuple[int, ...]` ellipsis**: the walker's base==8 loop consumed the
  `...` then re-entered tc_parse_type at `]` (a resync fire). The
  ellipsis is terminal: after it, the loop ends and the closing `]`
  is expected directly.

- **Field privacy is not a check-lane rule**: same-file private reads
  (private_field_access.li) and reads of imported sealee state
  (leak_imported_vault.li) are accepted by the host; the walker's two
  E900 gates were dropped (the E0202 method-existence gate stays,
  private_method_use.li remains both-reject).

Walker parity at commit time: official check harness 163/163,
AST 25 byte-exact, parser 20+5, MIR 42 byte-exact + 8/8 reject.
Verdict census across 465 li-files: from ~200 divergences to 14 residual.

## Residual verdict divergences (465-file census, 2026-10-18)

Walker-accepts / host-rejects (check-lane gates the walker has not
ported — extern-contract, array-index refinement bound, use-after-move
outside walker scope):

- cwe125_negative_index, cwe676_extern_no_contract, cwe787_dyn_index
- packages/li-gui/.../viewport_selection.li,
  packages/li-studio/.../studio_agent_invoke.li (use after move of
  `layout` across panel helpers)

Walker-rejects / host-accepts (walker-only residue gates whose host
counterpart passes silently painting "lic.error" for one lane):

- li-tests/codegen/ptr_array_strings.li, effects/async_missing_raises.li,
  effects/net_missing_raises.li, encapsulation/trait_missing_impl.li,
  net_trusted/seam_missing_net.li,
  packages/li-studio/.../studio_command_palette.li,
  packages/li-ui/.../studio_accessibility.li,
  packages/lig/li-tests/smoke/kernel_matmul_parity.li,
  packages/lig/src/lib.li

These stay as the next batch's targets (audit-source parity classes:
exit-code gates in extern/index policy and use-after-move expiry
scoping).

## Update 2026-10-18 (batch 4: check-lane index gates + extern seam + bounds)

Three walker-accepts / host-rejects classes ported; census 14 -> 11 (with
three former entries now matching and no new over-fires):

- **extern-contract seam** (cwe676): the policy lane fires on an
  `extern def strcpy` declaration without requires/ensures, mirroring
  policy.cpp's string-keyed rule (token scan, comment-independent).
- **Index gates** (cwe787 dyn, cwe125): ported typecheck.cpp's
  Expr::Kind::Index walk onto tc_postfix: constant rule (literal +/- unary
  folded negative vs the sized array code), and the Ident rule — a LONE
  ident index on a non-var parameter-array base must be a bounded int:
  refinement-typed params (alias rows encoding the upper bound as
  20000+N), for/parallel-for loop vars (registered 20000+for_end), and
  canonical while guards (`while i < <int lit>` marks the ident row).
- **Refinement typing**: `{v: int | ... v < N ...}` resolves to 20000+N
  (collect_refinement_bounds mirror, last conjunct match), binop results
  normalize refinement ids to plain int like the host's Ty::Int base,
  and the return unify compares normalized ids.

Mirror-correct exemptions that keep the walker from over-firing (all
probed): call/blob index exprs are not Idents (never fire, host Eq kind
Call), multi-line param όλtes var-array params (`!param.type.is_var`)
are exempt from the ident rule (encoded 29000+size), local arrays never
require bounded indexes, and unknown-end loop bounds accept (host -1
size sentinel).

Gates at this point: check harness 163/163, AST 25, parser 20+5, MIR
42 + 8/8. Residual 11 = the stable walker-only lic.error core
(ptr_array_strings, async/net_missing_raises, trait_missing_impl,
seam_missing_net, studio_compose/palette/accessibility trio,
kernel_matmul_parity, lig/src) plus the two use-after-move panel
fixtures (viewport_selection, studio_agent_invoke) — next batch.
