# Devbox Li development (engine and agents)

Short setup guide for **human developers** and **Cursor agents** working on the Li monorepo from a dedicated Linux dev machine (hostname `engine`, Debian 12).

See also: [Getting started — tools](getting-started-tools.md) (generic macOS/Linux), [httpd M1 baseline](../ecosystem/httpd-m1-baseline.md), [Cursor devbox host notes](../../../../scripts/README-devbox.md) (sleep/LUKS/RAM on the host).

## What you need

| Tool | Purpose |
|------|---------|
| **LLVM 18** + clang-18 | Build `lic` (`./scripts/build.sh`) |
| **cmake**, **ninja** | Li compiler build |
| **Node 24.16** (LTS), **npm** | `li-cursor-agents` SDK / httpd plan loop — pin via `LI_DEVBOX_NODE_VERSION` or `.node-version` in agents repo |
| **Python 3** | httpd TOML validators, routing oracles |
| **git**, **gh** | PR workflow |

Sibling repos (clone next to each other):

```text
~/Documents/Cursor/li-langverse/
  lic/                 # compiler + li-httpd runtime
  li-cursor-agents/    # Cursor SDK agents
  benchmarks/          # agent briefing / preflight
```

## One-time machine setup

### 1. System packages (sudo once)

```bash
cd ~/Documents/Cursor/li-langverse/lic
chmod +x scripts/setup-li-devbox.sh
sudo bash scripts/setup-li-devbox.sh --full
```

`--full` installs apt packages, user Node under `~/.local/node`, writes `~/.config/environment.d/99-li-dev.conf`, and runs `./scripts/build.sh`.

Without sudo, run only the user layer (Node + env file):

```bash
bash scripts/setup-li-devbox.sh --user
```

### 2. New shell

Log out/in or `source ~/.config/environment.d/99-li-dev.conf` so `PATH`, `LLVM_DIR`, `CC`, and `CXX` are set.

Verify:

```bash
which clang-18 node npm
echo "$LLVM_DIR"
cd ~/Documents/Cursor/li-langverse/lic
./build/compiler/lic/lic --version
```

### 3. Cursor / httpd agent env

Create `~/Documents/Cursor/.env` (never commit) with:

```bash
CURSOR_API_KEY=...
GH_TOKEN=...          # or GITHUB_TOKEN
LI_CONTROL_PLANE_STORE=disk
LI_SDK_TERMINAL_STREAM=1
LI_AGENT_MINIMAL_PROMPT=1
LI_CURSOR_AGENTS_ROOT=~/Documents/Cursor/li-langverse/li-cursor-agents
BENCHMARKS_ROOT=~/Documents/Cursor/li-langverse/benchmarks
LIC_ROOT=~/Documents/Cursor/li-langverse/lic
```

Build agents once:

```bash
export PATH="$HOME/.local/node/bin:$PATH"
cd ~/Documents/Cursor/li-langverse/li-cursor-agents
git checkout feat/goal-directed-sdk-loop   # or main after merge
npm ci && npm run build
```

## Daily commands

### Build compiler

```bash
cd ~/Documents/Cursor/li-langverse/lic
./scripts/build.sh
export LIC="$(./scripts/resolve-lic.sh)"
"$LIC" build --allow-open-vc li-tests/routing/match_routes.li -o /tmp/match_routes
```

### httpd M1 gates

**Full** (needs `lic` + optional `build/li-httpd`):

```bash
./scripts/httpd-plan-gates.sh
```

**Python-only** (no `lic` binary yet — CI smoke on constrained hosts):

```bash
HTTPD_GATES_SKIP_LIC_BUILD=1 HTTPD_RUN_BEARER_TEST=0 ./scripts/httpd-plan-gates.sh
```

Build the C httpd binary:

```bash
./scripts/build-li-httpd.sh
./scripts/test-auth-bearer.sh
```

### Autonomous httpd plan loop

Branches:

- **Implementation:** `cursor/httpd-plan-loop-54aa` → [PR #173](https://github.com/li-langverse/lic/pull/173)
- **Loop script:** `cursor/httpd-plan-loop-goal-directed` → [PR #172](https://github.com/li-langverse/lic/pull/172)
- **SDK:** `li-cursor-agents` `feat/goal-directed-sdk-loop` → [PR #9](https://github.com/li-langverse/li-cursor-agents/pull/9)

```bash
source ~/Documents/Cursor/.env
export PATH="$HOME/.local/node/bin:$PATH"
cd ~/Documents/Cursor/li-langverse/lic
git checkout cursor/httpd-plan-loop-54aa

./scripts/httpd-plan-loop.py --dry-run    # next todo + prompt
./scripts/httpd-plan-loop.py --once       # one code_implementer run
./scripts/httpd-plan-loop.py --max 15     # batch
```

The loop uses **`code_implementer`** with `--goal-file` (not `httpd_implementer`). It prefers **M1** plan todos over `w0`/`w1` language blockers unless `LI_HTTPD_PLAN_INCLUDE_BLOCKERS=1`.

Mark a todo done in loop state without running the agent:

```bash
./scripts/httpd-plan-loop.py --mark-done m1-routing-tests
```

## Agent rules (httpd)

1. Work on **`cursor/httpd-plan-loop-54aa`**; open/update **PR #173** — do **not** self-merge.
2. Run **`./scripts/httpd-plan-gates.sh`** before claiming done (or document `HTTPD_GATES_SKIP_LIC_BUILD=1` if only Python changed).
3. Update plan YAML todo status in `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` when a slice is truly complete.
4. Release notes under `docs/release-notes/` + `CHANGELOG.md`.
5. Do **not** merge stale httpd PRs (#87, #84, #130) wholesale.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `Set LLVM_DIR to LLVM 18` | `sudo bash scripts/setup-li-devbox.sh` or `export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm` |
| `npm: command not found` | `bash scripts/setup-li-devbox.sh --user` and `export PATH="$HOME/.local/node/bin:$PATH"` |
| `resolve-lic: no executable` | `./scripts/build.sh` |
| `build-li-httpd.sh failed` | Build `lic` first; on Linux ensure `runtime/li_rt_httpd.c` links (see `scripts/build-li-httpd.sh`) |
| Plan loop picks `w0-lean-gate` | Expected if no M1 todos open; set `LI_HTTPD_PLAN_INCLUDE_BLOCKERS=1` to include blockers |
| Disk full on long agent runs | `rm -rf li-cursor-agents/data/workspaces-test/*` |
| Cursor OOM | `bash ~/Documents/Cursor/scripts/limit-cursor-memory.sh --apply --percent 80` |

## Host notes (engine)

- **RAM:** ~64 GiB physical; swap ~1 GiB — see Cursor memory cap script in repo `Documents/Cursor/scripts/`.
- **SSH:** laptop → `engine` ([LAPTOP-SSH-SETUP.md](../../../../LAPTOP-SSH-SETUP.md)).
- **LUKS:** unattended reboot may need TPM unlock ([setup-devbox-always-on.sh](../../../../scripts/setup-devbox-always-on.sh)).
