# Self-hosted Li parser + runtime/CI hardening (2026-08-18)

## Self-hosted parser (milestone 2 of the Li front end)

`bootstrap/lic/main.li` now implements a validating recursive-descent parser in
Li, next to the self-hosted lexer (milestone 1). New `parse <file>` subcommand:
it re-dumps the token stream it consumed (same `kind<TAB>lexeme` format as
`lic lex`), then prints `parse ok` / `parse error` and exits 0 / 1 like the
C++ `lic parse`.

The parser is a faithful port of `compiler/parser/parser.cpp`'s grammar —
module decls, decorators, visibility, `def`/`extern def`/`async def`, type
aliases (typedict / enum / trait / object-of), all type forms (`array`,
`tuple` incl. named + variadic, `simd`, `Callable`, refinements, type params
with trait bounds), procs with params/raises/contracts, statements (if/elif/
else chains, while, for, parallel-for with contract blocks, borrow, discard,
var decls, returns, assignments, calls/method calls/indexing), imports with
dotted paths + aliases, error decls, and theorems/axioms/lemmas with
`prob_ensures` tails. The C++ parser's recovery behavior (a diagnostic is
recorded for every syntax error and `lic parse` fails if any exists) is
mirrored by a sticky error flag, so verdicts match exactly.

**Parity is measured, not assumed.** `scripts/check_li_parser_parity.sh`
(documented, wired into `ci.sh`):

- **Accept corpus (20 files)** covering every grammar feature — each must be
  accepted by both parsers, and the token stream the li parser consumes must
  byte-match `lic lex` (token-stream parity against the C++ parser).
- **Reject corpus (5 fixtures)** — syntactically invalid files each parser
  must reject, plus the tab-indent lexer error path.
- **Full-sweep mode** (`LI_PARSER_FULL_SWEEP=1`): every `*.li` in the repo —
  currently **482/482 exact accept/reject parity**.

The self-hosted binary is built by the C++ host with `--allow-open-vc`
(document why: the CLI plumbing procs leave their `ensures result == 0 or
result == 1` VCs open — same documented trade-off as the prover). The lexer
gate (`check_li_lexer_parity.sh`) still passes unchanged.

## Compiler fix: pointer-width args to user procs

`CallProc` lowering dropped pointer↔integer conversions for pointer-width
params (str/bytes/ptr are passed as `i64` in the user-call ABI), so a string
literal arg (`@.str` LLVM global) hit the verifier's "Call parameter type does
not match function signature". The argument coercion now includes
`ptr→i64` / `i64→ptr`, matching `CallExtern`. This is what lets Li source pass
string literals into user-defined helpers (`tok_is` in the bootstrap parser).

## Runtime net -Werror hygiene gate

`scripts/check-runtime-net-werror.sh` compiles `runtime/li_rt_net.c` with
`-Werror -Wall -Wextra` in a dedicated CI phase, catching platform-stub
hygiene regressions at compile time (the macOS `#else` branch previously
failed only at *link* time — e.g. the missing `epoll_wait_tagged_timeout_ms_i`
stub). To make the gate green on both platforms: `ptr_i` no longer drops
`const` into `memcpy`/`read`/`snprintf`/`recv`, and Linux-only dead statics
are marked `LI_RT_NET_UNUSED` instead of being silently unreferenced on
macOS.

## Interior-bounds ensures on unrolled physics kernels

`li-physics-weather` `diffuse_explicit` and `li-physics-em`
`poisson_jacobi_step` now carry `ensures` that the unrolled kernel writes only
interior cells 1..6 of the 8-cell array. The bound is expressed through
`diffuse_step_cell` / `poisson_step_cell` helpers (`requires 1 <= i <= 6`,
`ensures 1 <= result <= 6`) applied at constant indices, and the native VC
engine discharges every one of them with constant-index VCs
(`lic verify --no-lean-verify` exits 0 on both packages).

## Nightly full standalone sweep

`scripts/ci.sh` keeps the fast 20-package standalone subset for regular CI and
runs the FULL workspace member sweep when `LI_NIGHTLY=1`
(`scripts/ci-nightly.sh` is the documented entrypoint for the nightly job),
so every member package is verified to install, build, and run outside the
monorepo every night.
