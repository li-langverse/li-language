# Control flow and functions

## Functions (`def`)

```nim
def add(a: int, b: int) -> int
  requires true
  ensures result == a + b
  decreases 0
=
  return a + b
```

### Parameters

- Positional parameters with types.
- Generic parameters: `def id[T](x: T) -> T`.
- `extern proc` for C linkage (runtime or libraries).

### Return

- `return expr` or implicit on last expression (when supported).
- `-> unit` or no value for procedures that only run effects.

## Conditionals

```nim
if cond:
  ...
else:
  ...
```

Both branches must agree on effects and types where required.

## Loops

### `while`

```nim
while condition
  invariant <predicate>
  decreases <measure>
=
  body
```

`decreases` is mandatory.

### `parallel for`

```nim
parallel for i in 0..<N
  requires disjoint_...
  decreases N - i
=
  body
```

See [SIMD and parallel](simd-parallel.md).

## Variables

```nim
var x: int = 0
var row: array[64, float]
```

`let` / immutability rules follow the borrow checker (v1 focuses on `var` and borrows).

## Borrowing (memory)

```nim
def use(readonly: imm array[N, int]) -> unit
def fill(writable: mut array[N, int]) -> unit
```

| Modifier | Meaning |
|----------|---------|
| `imm` | Shared read-only reference |
| `mut` | Exclusive mutable reference |

Double `mut` or use-after-move is rejected. Tests: `li-tests/borrow/`.

## Assignment

```nim
x = x + 1
grid[i] = value
```

Index assignment on arrays requires valid indices.

## `print` and expressions

`print` is a built-in for simple output. General expressions include arithmetic, calls, and indexing.

## `cast` with proof

```nim
cast[i32](x, proof_term)
```

Bare `cast` without proof is forbidden.
