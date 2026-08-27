# Collections and generics

Li mirrors **Python 3.14** collection typing where possible, compiled to native layouts.

## `list[T]`

```nim
def sum(xs: list[int]) -> int
```

Dynamic length; allocation may `raises Alloc`.

## `dict[K, V]`

```nim
def lookup(m: dict[str, int], key: str) -> int
```

Arity and key types are checked statically.

## `tuple` and named tuples

```nim
type Pair = tuple[a: int, b: float]
```

Variadic tuples supported in tests (`li-tests/collections/`).

## `TypedDict`

```nim
type Config = TypedDict
  host: str
  port: int
```

## `enum`

```nim
type Mode = enum
  Off, On, Auto
```

## Generics (PEP 695)

```nim
def box[T](x: T) -> T
  requires true
  ensures result == x
  decreases 0
=
  return x
```

Type parameters in square brackets on `def` and `type`.

## `Protocol` and `Callable`

Structural protocols (e.g. `Sized`) and function types for higher-order code.

Tests: `li-tests/generics/`.

## Arrays vs lists

| | `array[N, T]` | `list[T]` |
|---|---------------|-----------|
| Size | Fixed at compile time | Grows at runtime |
| Stack / inline | Often stack-allocated | Heap |
| HPC kernels | Preferred | Less common in hot paths |
