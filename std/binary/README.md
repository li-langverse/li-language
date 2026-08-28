# `std/binary` — bit-packed values

**Language reference:** [Scalar precision, literals, and binary data](../../docs/language/scalar-precision.md#binary-type-binary)

## Purpose

- **`binary`** — bit masks, quantized weight planes, custom packings (`0b1010…` literals).
- **`bytes`** — byte buffers for I/O and UTF-8 (`std/bytes/`).

Do not use `bytes` when you mean a semantic bit vector; use `binary`.

## Surface (`binary.li`)

| Symbol | Role |
|--------|------|
| `binary_tag()` | Version/smoke int for composable imports |

## Literals

```nim
var mask: binary = 0b10110100
```

## Status

| Feature | Status |
|---------|--------|
| Type + `0b` literals | Typecheck ✓ |
| Bitwise operators | Roadmap |
| MIR / runtime layout | Roadmap |

## Physics

`physics.core` preset `precision_binary_weights()` sets `ScalarPrecision.weights_encoding = 1` when simulations use binary-packed weights.
