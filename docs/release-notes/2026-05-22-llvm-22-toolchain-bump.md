# Release notes: LLVM 22 toolchain bump

## Summary

Org pin moves from **LLVM 18 → LLVM 22** for `lic` codegen/CI. Adds `scripts/llvm-env.sh` and `scripts/ci-install-llvm.sh` so Debian bookworm (no `clang-18` in main) and Ubuntu GHA install via apt.llvm.org when needed.

**Note:** Windows CI previously used Chocolatey **18.1.7**, not 22 — now targets **22.1.0** with the same `LLVMConfig.cmake` discovery path.

## Agent continuation

1. Devbox: `sudo bash scripts/ci-install-llvm.sh` then `./scripts/build.sh`
2. Local CI: `./scripts/local-ci.sh` (native or `--docker`)
3. Rebuild after pull — delete `build/` if CMake caches old LLVM

## Changed

| Path | Change |
|------|--------|
| `CMakeLists.txt` | Require `LLVM_VERSION_MAJOR` 22 |
| `scripts/llvm-env.sh` | Central detect (`LI_LLVM_MAJOR`, brew/apt paths) |
| `scripts/build.sh` | Source `llvm-env.sh` |
| `scripts/ci-install-llvm.sh` | CI/devbox LLVM install |
| `.github/workflows/*`, package `ci.yml` templates | clang/llvm 22 |
| `docs/guide/getting-started-tools.md` | Debian bookworm + apt.llvm.org |

## Not changed

- Compiler IR logic (LLVM C++ API — verify on first build)
- Org mirror repos outside `lic` (may still say 18 until synced)

## Breaking

**Contributors** must install LLVM **22** (not 18). Old `build/` trees may need `rm -rf build`.

## Downstream

- **li-local-ci** / **benchmarks** scripts updated in sibling commits when present
- **roadmap/agent-kit** skill text: use `llvm.sh 22` on Debian
