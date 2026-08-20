# Getting started — tools

You need a C++ compiler, CMake, Ninja, and **LLVM 22** once. After that, installing Li is one command.

## Install the compiler (one command)

Once the toolchain is present, this builds the compiler, installs it, and wires its bin directory into your shell profile:

```bash
./scripts/install.sh
```

That installs `lic` to `~/.local/bin/lic` and adds it to `~/.zshrc`, `~/.bashrc`, or `~/.config/fish/config.fish` depending on your shell. Open a new terminal and verify:

```bash
lic --version
```

| Variable | Effect |
|----------|--------|
| `LI_INSTALL_PREFIX=/usr/local` | Install to `/usr/local/bin/lic` |
| `LI_INSTALL_BIN=/custom/bin` | Install to a specific bin directory |
| `LI_NO_PROFILE=1` | Skip the shell-profile PATH wiring |

The standard CMake install path is also supported:

```bash
cmake --build build
cmake --install build   # -> /usr/local/bin/lic (or set -DCMAKE_INSTALL_PREFIX)
```

## macOS

```bash
brew install llvm@22 cmake ninja
export LLVM_DIR="$(brew --prefix llvm@22)/lib/cmake/llvm"
export CC=clang CXX=clang++
./scripts/build.sh
./build/compiler/lic/lic --version
```

## Linux (Ubuntu 24.04+)

```bash
sudo apt-get install cmake ninja-build clang-22 llvm-22-dev lld-22
export LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm
export CC=clang-22 CXX=clang++-22
./scripts/build.sh
```

If `clang-22` is not found, use [apt.llvm.org](https://apt.llvm.org/):

```bash
wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
sudo /tmp/llvm.sh 22
sudo apt-get install -y clang-22 llvm-22-dev lld-22
```

## Linux (Debian 12 bookworm)

Debian main repos do not ship LLVM 22; use apt.llvm.org:

```bash
sudo apt-get install -y cmake ninja-build wget gnupg zlib1g-dev libzstd-dev python3
wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
sudo /tmp/llvm.sh 22
sudo apt-get install -y clang-22 llvm-22-dev lld-22
export LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm
export CC=clang-22 CXX=clang++-22
./scripts/build.sh
```

Or `./scripts/build.sh` after `export LLVM_DIR=...` — it auto-detects via `scripts/llvm-env.sh`.

## Linux (Debian 12 / Ubuntu)

```bash
sudo apt-get install cmake ninja-build clang-18 llvm-18-dev lld-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export CC=clang-18 CXX=clang++-18
./scripts/build.sh
```

**Dedicated dev box (e.g. `engine`):** use the idempotent script and agent-oriented guide — [devbox Li development](devbox-li-development.md).

```bash
sudo bash scripts/setup-li-devbox.sh --full
```

## Windows

Use the GitHub Actions recipe as a reference: LLVM 22 via Chocolatey, then `cmake -B build` with `LLVM_DIR` pointing at the install.

## Lean 4 (proof gate — optional for quick builds, required for full CI)

```bash
bash /home/s4il0r/Documents/Cursor/li-langverse/lic/scripts/ci-install-lean.sh
export PATH="$HOME/.elan/bin:$PATH"
cd /home/s4il0r/Documents/Cursor/li-langverse/lic/docs/semantics && lake build
```

Without `lake`, `lic build` still runs but skips semantics verification (see [provability-gaps.md](../verification/provability-gaps.md)).

## Your first build

```bash
lic build examples/hello.li -o hello --release
./hello
```

## Commands you will use

| Command | What it does |
|---------|----------------|
| `lic parse file.li` | Is the syntax OK? |
| `lic check file.li` | Syntax + types (quick, for the editor) |
| `lic build file.li -o app` | Full pipeline → runnable program |
| `lic build file.li -o app --release` | Optimized native binary |
| `lic build file.li -o app --threads=8` | Hint OpenMP to use 8 threads (when parallel code is present) |

`lic check` is for speed while you type. **`lic build` is the real gate** when you want a program you trust.

## Run the project’s tests

```bash
./scripts/ci.sh
```

That builds Li, runs security checks, and runs the full `li-tests` suite.

Next: [Hello world in depth](hello-world.md).
