# Autoresearch: `horner_pure_li` lexer regression (2026-05-18)

## Hypothesis

The sole **pure_li** red row (`horner_pure_li`, ~88× vs cpp) was caused by a codegen/MIR defect in float Horner lowering.

## Falsification

Lexer audit: `compiler/lexer/lexer.cpp` mapped **`+` → `TokenKind::Minus`**, so `i = i + 1` became subtraction and the `while i < 5_000_000` loop did not match the reference kernel semantics.

## Fix

Emit `TokenKind::Plus` for `+`; keep `->` arrow handling on `-` only.

## Evidence (local, AppleClang, `-O3 -march=native`)

| Build | cpp median | li median | li/cpp |
|-------|------------|-----------|--------|
| Before | 0.0110 s | 0.7860 s | **71.7×** |
| After | 0.0102 s | 0.0026 s | **0.26×** |

Harness: `benchmarks/tier1_micro/horner_pure_li` via `benchmarks/harness/bench.py` helpers.

## Contracts / Lean

N/A — lexer-only; no `trusted.lean` or new axioms.

## Follow-up

- [x] Anti-DCE in `horner_pure_li/li/main.li` (`li_rt_volatile_sink_f64`) — harness `verify_checksum` rejects pure_li if li < 0.45× native (DCE guard).
- Ingest row into org `benchmarks` catalog after merge (normal ingest only).

**north_star_fit:** scientific computing / micro-kernel codegen (**PH-5b**, **PH-7e**).
