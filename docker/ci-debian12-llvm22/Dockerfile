# Dev/CI toolchain only — NOT a release image with a baked-in `lic` binary.
# The repo is mounted at /src; ./scripts/build.sh (or ci.sh) builds lic inside the container.
# Publish: .github/workflows/publish-docker-ci-image.yml → ghcr.io/li-langverse/lic-ci:debian12-llvm22
FROM debian:bookworm-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    cmake ninja-build wget gnupg ca-certificates curl pkg-config \
    lsb-release software-properties-common \
    zlib1g-dev libzstd-dev python3 python3-pip git rsync \
  && (apt-get install -y --no-install-recommends clang-22 llvm-22-dev lld-22 \
      || (wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh \
          && chmod +x /tmp/llvm.sh \
          && /tmp/llvm.sh 22 \
          && apt-get install -y --no-install-recommends clang-22 llvm-22-dev lld-22)) \
  && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh
WORKDIR /src
ENV LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm
ENV CC=clang-22
ENV CXX=clang++-22
ENV LI_LLVM_MAJOR=22
ENV CI=true
