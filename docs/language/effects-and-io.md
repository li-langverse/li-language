# Effects and I/O

Li tracks **effects** explicitly — what a procedure is allowed to do besides pure computation.

## Common effects

| Effect | Meaning |
|--------|---------|
| `raises IO` | Console, files, environment |
| `raises Alloc` | Heap allocation (`list`, `dict`, …) |
| `raises DivZero` | Division can fail |
| `raises Async` | Async/await (roadmap) |

## Declaring effects

```nim
def main() raises IO -> int
  requires true
  ensures result == 0
  decreases 0
=
  print("hello")
  return 0
```

A procedure that uses I/O without `raises IO` fails to compile.

## Trusted I/O axioms

Low-level I/O is axiomatized in `docs/semantics/trusted.lean`. User code proves against those axioms; it does not redefine them.

## `extern` procedures

```nim
extern proc puts(s: str) -> int

def main() raises IO -> int
  ...
=
  puts("from C runtime")
  return 0
```

Links to C symbols when building.

## Command-line args (runtime)

The runtime may expose argc/argv helpers (`li_rt_argc`, `li_rt_argv`) for drivers and benchmarks.

Tests: `li-tests/effects/`, `li-tests/runtime/`.
