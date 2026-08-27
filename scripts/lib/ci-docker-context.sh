# shellcheck shell=bash
# Map LI_CI_DOCKER_IMAGE tag → Dockerfile directory under docker/
ci_docker_build_context() {
  local root="$1"
  local image="${2:-}"
  case "$image" in
    *ubuntu24*)
      echo "$root/docker/ci-ubuntu24-llvm22"
      ;;
    *debian12*|*)
      echo "$root/docker/ci-debian12-llvm22"
      ;;
  esac
}
