# Hello world

Li programs are made of **procedures** (`def`). Each function states what it needs, what it guarantees, and how it stops.

## Minimal program

```nim
def main() raises IO -> int
  requires true
  ensures result == 0
  decreases 0
=
  print("Hello from Li")
  return 0
```

| Line | Meaning |
|------|---------|
| `def main() raises IO -> int` | Entry function; prints, so it declares `raises IO`, and returns an integer exit code |
| `requires true` | Precondition (here: always allowed to run) |
| `ensures result == 0` | Postcondition: return value is 0 |
| `decreases 0` | This procedure does not loop — trivially finishes |
| `=` | Start of the body (indentation continues the block) |

## Build

```bash
lic build hello.li -o hello
./hello
```

## What “requires / ensures / decreases” are for

Think of them as **promises**:

- **requires** — “I only run when this is true.”
- **ensures** — “When I finish, this will be true.”
- **decreases** — “Something counts down so I cannot loop forever.”

Li uses these promises during **`lic build`**. If Li cannot see that your promises are consistent with your code, the build fails. That is intentional: you learn about mistakes before you run anything.

## Printing text

`print` works on integers and strings (when the runtime supports them):

```nim
def main() raises IO -> int
  requires true
  ensures result == 0
  decreases 0
=
  print(42)
  return 0
```

## Calling other procedures

```nim
def greet() raises IO -> int
  requires true
  ensures result == 0
  decreases 0
=
  print("Hi")
  return 0

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  greet()
  return 0
```

Next: [Examples gallery](examples-gallery.md) or the [Language handbook](../language/overview.md).
