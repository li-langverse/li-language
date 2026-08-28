# MIR int literal compare + build argv order

## Summary

Fix `BinOpInt` lowering so integer literals on either side of `!=` / `==` are not mistaken for literal zero, and parse `lic build` input path after proof/compile flags.

## Agent continuation

1. **Read:** `compiler/mir/lower.cpp` (`Expr::Kind::BinOp`), `compiler/mir/include/li/mir.hpp` (`rhs_is_literal` default), `compiler/lic/main.cpp` (`build` argv loop).
2. **Run:** `cmake --build build -j$(nproc)`; `./build/lic build li-tests/compile_ok/int_ne_literal.li -o /tmp/int_ne`; `/tmp/int_ne` → exit 0; `./build/lic build --allow-open-vc li-tests/routing/match_routes.li -o /tmp/mr` (if http import resolves).
3. **Then:** rebase/merge PR superseding #159; run `./li-tests/run_httpd_config.sh` on main.
4. **Blocked on:** full `sqrt_open_bound` proof (P-float) and removing `sorry` on `mat2_at2_float_spec_proved`.

## Changed

| Area | Path | Evidence |
|------|------|----------|
| MIR | `compiler/mir/lower.cpp` | `lhs_is_literal` / `rhs_is_literal` set from `IntLit` |
| CLI | `compiler/lic/main.cpp` | first non-flag token is build input |
| Test | `li-tests/compile_ok/int_ne_literal.li`, `li-tests/manifest.toml` | `r != 1` returns 0 |

## Not changed

- Proof CLI policy (`--allow-open-vc`, `--no-lean-verify`) — see `2026-05-22-proof-cli-flags.md`.
- HTTPd routing semantics or `li_rt_net` C code.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A — bugfix |
| **Security** | N/A |
| **Performance** | N/A |
| **Downstream** | Benchmarks ingest unchanged |
