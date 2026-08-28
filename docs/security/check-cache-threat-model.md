# `lic check` cache threat model

## Trusted

- The **compiler binary** (`lic`) and **source files** the user asks to check.
- **`li.toml` `[check]`** config at the workspace root (typosquat level, etc.).

## Untrusted (must not affect correctness)

| Input | Risk | Mitigation |
|-------|------|------------|
| Pre-existing files under `--cache-dir` | Cache poisoning, path escape | `v=1` format; reject malformed/oversized (>1 MiB); keys are hex-only; entries resolved under canonical cache root; no symlink reads/writes |
| `--cache-dir` pointing at a symlink | Write outside intended dir | Canonical root; entry paths must stay under root; skip symlink targets |
| `--cache-max-mb=0` or huge values | Unbounded disk / bypass LRU | Default 64 MiB when unset; hard cap 4096 MiB |
| `--jobs` / `--max-memory` / `--threads` | Fork / RSS DoS | Caps: jobs≤256, memory≤65536 MiB, threads≤256 (warnings when clamped) |
| Shared `build-dir` across workers | Artifact races | Workspace/check pool uses subprocesses; build isolation is `--build-dir` per worker (WP2) |

## Cache key material

Each entry is keyed by FNV-1a hashes of:

1. Main file content  
2. `check_config_hash` (`li.toml` typosquat level)  
3. Direct import graph (sorted import paths + file contents)  
4. Absolute source path  
5. Compiler version (`LI_VERSION`)  
6. Output mode (`human` vs JSON command name)

Changing any of these invalidates the entry.

## Entry format

```text
v=1
exit=<int>
content=<hex64 main-file fnv>
---
<diagnostics payload>
```

Loads reject entries whose `content=` line does not match the current file hash (blocks fake `exit=0` poison).

Diagnostics-only JSON (no AST/MIR). Entries larger than **1 MiB** are not stored and are deleted on read.

## Evidence

```bash
./scripts/build.sh
./li-tests/cache_exploits/check_cache_exploits.sh
./scripts/ci-security.sh
```
