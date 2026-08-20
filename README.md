# Li

**理** — *reason, principle.*

Li is a language for people who want their programs to be **correct before they are fast**, and **clear before they are clever**.

You write ordinary-looking code. Before anything runs, Li checks that your promises about the program hold — like a careful reviewer who never gets tired. When that check passes, you get a real program that can use many CPU cores and vector math **without** bolting on extra libraries for parallelism or speed.

---

## Install

One command builds the compiler, installs it, and wires it onto your `PATH`:

```bash
./scripts/install.sh
```

That installs `lic` to `~/.local/bin/lic` and adds it to your shell profile (`~/.zshrc`, `~/.bashrc`, or `~/.config/fish/config.fish`). Open a new terminal and check it:

```bash
lic --version
```

Other destinations, or skip the PATH wiring:

```bash
LI_INSTALL_PREFIX=/usr/local ./scripts/install.sh   # -> /usr/local/bin/lic
LI_NO_PROFILE=1 ./scripts/install.sh                 # leave your shell profile untouched
```

The standard CMake install path works too:

```bash
cmake --build build
cmake --install build   # -> /usr/local/bin/lic (or set -DCMAKE_INSTALL_PREFIX)
```

The toolchain (C++ compiler, CMake, Ninja, LLVM 22) must be present once first — see [getting started](docs/guide/getting-started-tools.md).

---

## Hello, Li

Save this as `hello.li`:

```nim
def main() raises IO -> int
  requires true
  ensures result == 0
  decreases 0
=
  print("Hello from Li")
  return 0
```

Build and run (after [installing](docs/guide/getting-started-tools.md) once; dev box: [devbox Li development](docs/guide/devbox-li-development.md)):

```bash
lic build hello.li -o hello
./hello
```

Every Li program includes small **promises** (`requires`, `ensures`, `decreases`). They are not comments — they are what Li uses to know your program makes sense.

---

## A slightly bigger example

Counting with a loop:

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var total: int = 0
  var n: int = 0
  while n < 1000
    total = total + n
    n = n + 1
  return 0
```

Li insists that loops say how they finish (`decreases`) so endless loops cannot slip through by accident.

---

## Fast math (write math, not intrinsics)

On fixed `array[N, float]` tiles, use **`dot(x, y)`** or **`x @ y`** — the compiler lowers to fast loops (and SIMD at `-O3`):

```li
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var x: array[4, float]
  var y: array[4, float]
  var i: int = 0
  while i < 4
    x[i] = 1.0
    y[i] = 2.0
    i = i + 1
  var s: float = x @ y
  return 0
```

More: [Math-first HPC examples](docs/guide/math-hpc-examples.md) · [Linear algebra](docs/language/linear-algebra.md).

You do **not** need NumPy, a special C extension, or a separate vector library for this — it is part of the language.

More examples: [Vector and parallel guide](docs/guide/fast-math-and-parallelism.md).

---

## Many cores, safely

Use all your CPU cores only when Li can see that threads will not fight over the same memory:

```nim
def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var buf: array[8, float]
  var i: int = 0
  while i < 8
    buf[i] = 1.0
    i = i + 1
  parallel for j in 0..<8
    requires disjoint_elem(j, buf)
    decreases 8 - j
  =
    buf[j] = 0.0
  return 0
```

If two threads would write the same slot, **the build stops** — you fix it before running, not after a mysterious crash.

---

## Three ideas that define Li

| Idea | In plain words |
|------|----------------|
| **Prove it** | Wrong programs are rejected at build time, not discovered in production. |
| **Write it easily** | Reads like prose — Python-style clarity, Nim-like layout; see [philosophy](docs/language/philosophy.md). |
| **Run it fast** | After proof, the same code becomes native speed with vectors and multiple cores. |

Proof always comes first. Speed never skips the check.

---

## Learn more

| I want to… | Start here |
|------------|------------|
| Install tools and build Li | [Getting started (tools)](docs/guide/getting-started-tools.md) |
| See more copy-paste examples | [Examples gallery](docs/guide/examples-gallery.md) |
| Learn the whole language | [Language handbook](docs/language/overview.md) |
| Naming & simplicity | [Philosophy](docs/language/philosophy.md) |
| Game worlds (vision) | [World Studio](docs/game-dev/world-studio-vision.md) |
| Understand the build steps | [How `lic build` works](docs/compiler/build-pipeline.md) |
| Understand why this is “mathematically provable” | [Why Li is provable](docs/compiler/why-provable.md) |
| See every test and security check | [Tests & audits](docs/testing/overview.md) |
| Read the full design spec (technical) | [Language design spec](docs/superpowers/specs/2026-05-14-li-language-design.md) |

Published docs site: [li-langverse.github.io/li-language](https://li-langverse.github.io/li-language/)

Create a new package: `./scripts/li-new-package <name> --kind library` — see [Creating packages](docs/guide/creating-packages.md).

---

## License

MIT OR Apache-2.0 — use Li in open or closed projects under either license.
