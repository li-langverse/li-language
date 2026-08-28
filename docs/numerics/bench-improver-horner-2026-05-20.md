# Bench improver pass — `horner_pure_li` (2026-05-20)

## Dashboard vs harness

| Source | `horner_pure_li` li/cpp | Notes |
|--------|-------------------------|-------|
| Public `summary.json` (2026-05-16 ingest) | **88.82×** | Stale: pre-`+` lexer fix; loop did not increment |
| Local tier-1 (DCE, `acc` unused) | **0.57×** | Bogus green — LLVM deleted the Horner loop |
| Local tier-1 (this pass, `li_rt_volatile_sink_f64`) | **10.7×** | Honest pure-Li scalar codegen vs native C reference |

Evidence rows (`benchmarks/results/latest.csv` on this branch):

```csv
horner_pure_li,cpp,...,wall_time,0.0009,s,...
horner_pure_li,li,...,wall_time,0.0096,s,...
```

## Changes

1. **`li_rt_volatile_sink_f64`** — runtime volatile store (matches cpp `volatile` checksum sink).
2. **`horner_pure_li/li/main.li`** — call sink after loop; `raises IO` on `main`.
3. **`bench.py`** — pure_li verify fails if `li_time < 0.45 × native` (DCE guard).
4. **`bench.py` (2026-05-22)** — `horner_pure_li` also requires Li checksum == native == Python ref (`LI_PRINT_SINK_F64=1`); tier-1 verify errors abort the run (no warn-and-continue).

## Status vs ≤1.2× cpp gate

Still **red** (~10.7×) before PH-7e follow-up. **2026-05-22:** needs-based runtime link (no `li_rt_net` in microbenches), `HornerFmaUnroll` / `HornerStepPow4` (const `x`), non-IO `volatile_sink` — devbox tier-1 ~**3×** cpp (honest, passes DCE guard). Lexer `Plus` fix: [autoresearch-horner-lexer-2026-05-18.md](./autoresearch-horner-lexer-2026-05-18.md).

## Near-limit tier-2 (deferred)

`matmul_blocked`, `nbody_gravity`, `double_pendulum`, `wave_equation_1d`, `harmonic_oscillator_chain` are 1.02–1.03× on stale dashboard — shared C kernels; no harness change in this PR.
