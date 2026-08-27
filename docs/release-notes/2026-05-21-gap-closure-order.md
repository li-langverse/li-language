# Gap closure order — 2f loop witness, 2i-b norm, 7d-c AST races, H routing contract

## Summary

Implements the master-plan priority slice: **P-linalg loop dot witness**, prelude **`norm`**, **AST parallel policy** (replacing string heuristics), and an **httpd routing contract** smoke test.

## Agent continuation

1. **Read** [provability-gaps.md](../verification/provability-gaps.md#still-open-report-every-session) — report which **G-*** remain open every session.
2. **Run** `./li-tests/tooling/discharge_linalg_int_lean.sh`, `./li-tests/run_all.sh --ci` (**180** pass on this branch).
3. **Next** — **G-lean** default kernel gate; tier-1 **≤1.2× C++** (`bench.py --tier 1`); float `vec3_dot` Props; httpd reactor.
4. **Blocked on** remote bench env for perf sign-off; `Core.lean` for full loop semantics in Lean.

## Changed

| Area | Paths |
|------|--------|
| **2f** | `vc_witness.cpp` (`witness_dot4_int_loop`), `linalg_dot4_int_loop_open` → `verify_ok` |
| **2i-b** | `norm` prelude + typecheck + MIR (`li_rt_sqrt` / int sum-of-squares); `math_linalg/norm_*.li` |
| **7d-c** | `policy_module.cpp` AST race checks; `policy.cpp` parallel strings removed |
| **H** | `httpd/route_match_contract.li` |
| **Docs** | `provability-gaps.md`, `proof-corpus-roadmap.md` |

## Not changed

- Tier-1 perf threshold enforcement (still advisory via `LI_HPC_COMPETITIVE_STRICT=0`).
- Default Lean kernel in `lic build` for all modules.

## Breaking

N/A.

## Security

Parallel races rejected via AST (**E0320** / **E0350**) instead of source substring scans.

## Performance

`norm` uses `ArrayDotF64` + `li_rt_sqrt` for float arrays.

## Downstream

- **benchmarks** ingest unchanged until lic tag after merge.
