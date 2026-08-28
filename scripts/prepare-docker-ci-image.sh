#!/usr/bin/env bash
# Ensure the lic CI Docker image exists locally (pull public GHCR, else build).
# No credentials are read or stored — publish uses GITHUB_TOKEN only in GHA.
#
# Usage:
#   ./scripts/prepare-docker-ci-image.sh          # pull or build if missing
#   ./scripts/prepare-docker-ci-image.sh --pull   # pull only (fail if unavailable)
#   ./scripts/prepare-docker-ci-image.sh --build  # build from docker/ci-debian12-llvm22 (default)
#
# Env:
#   LI_CI_DOCKER_IMAGE  default ghcr.io/li-langverse/lic-ci:debian12-llvm22
#   (use :ubuntu24-llvm22 for GHA ubuntu-24.04 base parity)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${LI_CI_DOCKER_IMAGE:-ghcr.io/li-langverse/lic-ci:debian12-llvm22}"
MODE="${1:-}"

# shellcheck source=lib/container-runtime.sh
source "$ROOT/scripts/lib/container-runtime.sh"
# shellcheck source=lib/ci-docker-context.sh
source "$ROOT/scripts/lib/ci-docker-context.sh"

CTR="$(resolve_container_runtime)" || {
  echo "prepare-docker-ci-image: need podman or docker (podman preferred when available)" >&2
  exit 1
}

image_ready() {
  "$CTR" image inspect "$IMAGE" >/dev/null 2>&1
}

if image_ready; then
  echo "prepare-docker-ci-image: already present $IMAGE"
  "$CTR" image inspect "$IMAGE" --format '  size={{.Size}} created={{.Created}}'
  exit 0
fi

if [[ "$MODE" != "--build" ]]; then
  echo "prepare-docker-ci-image: pulling $IMAGE"
  if "$CTR" pull "$IMAGE"; then
    echo "prepare-docker-ci-image: ok (pulled)"
    exit 0
  fi
  if [[ "$MODE" == "--pull" ]]; then
    echo "prepare-docker-ci-image: pull failed (image may not be published yet)" >&2
    exit 1
  fi
  echo "prepare-docker-ci-image: pull failed — building locally (set LI_CI_DOCKER_BUILD=1 to skip pull)" >&2
fi

BUILD_CTX="$(ci_docker_build_context "$ROOT" "$IMAGE")"
echo "prepare-docker-ci-image: building $IMAGE from $BUILD_CTX"
"$CTR" build -t "$IMAGE" "$BUILD_CTX"
echo "prepare-docker-ci-image: ok (built via $CTR)"
"$CTR" image inspect "$IMAGE" --format '  size={{.Size}}'
