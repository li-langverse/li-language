# Local CI — prebuilt Docker images (GHCR)

**Goal:** New machines run `./scripts/local-ci.sh --docker` without apt/llvm install on every run.

## What is in the image (today)

- OS base (Debian 12 or Ubuntu 24.04)
- **LLVM 22**, clang, cmake, ninja, python3, git, rsync — enough to **build `lic` from source**

## What is not in the image (by design)

- **No pinned or prebuilt `lic` compiler** — version comes from the mounted checkout; `ci.sh` runs `scripts/build.sh` every time.
- No Lean/elan (optional on host; GHA installs lake separately)
- Future **release images** with a baked `lic` + runtime are **deferred** until the compiler is stable (see master plan § deferred release containers).

## Image tags

| Registry | Tag | Contents |
|----------|-----|----------|
| `ghcr.io/li-langverse/lic-ci` | **`debian12-llvm22`**, `latest` | **Debian 12 (bookworm)** — preferred for local/podman |
| `ghcr.io/li-langverse/lic-ci` | `ubuntu24-llvm22` | Ubuntu 24.04 — optional GHA `ubuntu-24.04` base parity |

Built from `docker/ci-debian12-llvm22/` or `docker/ci-ubuntu24-llvm22/` (LLVM 22 via apt or apt.llvm.org, same as `scripts/ci-install-llvm.sh`).

## On a new machine

```bash
cd lic
./scripts/prepare-docker-ci-image.sh   # docker pull (public) or local build
./scripts/local-ci.sh --docker
```

Override image:

```bash
export LI_CI_DOCKER_IMAGE=ghcr.io/li-langverse/lic-ci:debian12-llvm22
# GHA parity: .../lic-ci:ubuntu24-llvm22
```

**Container runtime:** scripts prefer **`podman`** when `podman info` works, else `docker`. Override with `CONTAINER_RUNTIME=docker` if needed.

**Docker group:** for Docker Engine, user must access `/var/run/docker.sock` (e.g. `sudo usermod -aG docker $USER` then re-login). No tokens are stored in the repo.

**In-container note:** `local-ci.sh --docker` sets `HTTPD_SKIP_AUTH_BEARER_SMOKE=1` (TCP smoke to `127.0.0.1` is unreliable in containers). Config/routing gates still run via `run_httpd_config.sh`.

## Publish (maintainers)

Workflow `.github/workflows/publish-docker-ci-image.yml` pushes to GHCR using **`GITHUB_TOKEN`** in Actions only. Do not commit `GHCR_TOKEN`, `.env` credentials, or `docker login` output.

Trigger manually: **Actions → Publish CI Docker image → Run workflow**.

After the first publish, `podman pull ghcr.io/li-langverse/lic-ci:debian12-llvm22` works without login for public packages.

## OS coverage

| Layer | Coverage |
|-------|----------|
| `debian12-llvm22` (default) | Linux dev boxes / podman (Debian 12) |
| `ubuntu24-llvm22` | Closer to GHA `ubuntu-24.04` job base image |
| `./scripts/local-ci.sh` (native) | Host OS (Linux or macOS) |
| GHA `macos-14` / `windows-latest` | Not in this image — still need GHA or real runners |

### Can you add macOS / Windows images and run them on Linux?

**No — not for real GHA parity.**

| Target | Docker on Linux devbox |
|--------|-------------------------|
| **Linux** (Debian 12 default) | Yes — `ghcr.io/li-langverse/lic-ci:debian12-llvm22` |
| **Linux** (GHA ubuntu job) | Optional — `ghcr.io/li-langverse/lic-ci:ubuntu24-llvm22` |
| **macOS** (`macos-14`) | No official macOS container OS; Apple license + no Docker-equivalent macOS runner image |
| **Windows** (`windows-latest`) | Windows *containers* need a **Windows Docker host** (different engine than Linux containers). They do not run on normal Linux Docker and are not the same as GHA’s bash+LLVM Windows VM |

What people sometimes confuse:

- **Linux image on a Mac/Windows laptop** — works (Docker runs the same Ubuntu image). That is still **Linux CI**, not macOS/Windows CI.
- **`act -P macos-14=...` on Linux** — still executes **Linux** runner images; macOS workflow jobs are skipped or fail unless you use a real Mac host.
- **Cross-compile to Windows from Linux** — optional extra; lic’s Windows job is “build on Windows with MSVC/clang + LLVM CMake,” not fully replaceable by a Linux container.

**Practical matrix for lic:**

1. **Linux devbox** — native `local-ci.sh` or `--docker` (Ubuntu image).
2. **Mac dev machine** — native `local-ci.sh` (Homebrew LLVM; matches `build-and-test-macos` toolchain choices).
3. **Windows dev machine** — native bash CI or GHA.
4. **All three OS jobs green** — GitHub Actions (or self-hosted runners on real Mac/Win hardware).

Publishing more GHCR tags only helps for **Linux** variants (e.g. `ubuntu24-llvm22`, optional slim vs full). macOS/Windows “images” for local Actions are not a supported substitute on Linux.

## li-local-ci

Set `LI_LOCAL_CI_IMAGE=ghcr.io/li-langverse/lic-ci:debian12-llvm22` in profile `lic-docker` (see `li-local-ci` repo) for act/profile container runs.
