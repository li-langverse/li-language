# Dev/CI toolchain only — no prebuilt `lic` (see ci-debian12-llvm22/Dockerfile header).
# Publish: .github/workflows/publish-docker-ci-image.yml → ghcr.io/li-langverse/lic-ci
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    cmake ninja-build wget gnupg ca-certificates curl pkg-config lsb-release \
    software-properties-common \
    zlib1g-dev libzstd-dev python3 python3-pip git rsync \
  && wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh \
  && chmod +x /tmp/llvm.sh \
  && /tmp/llvm.sh 22 \
  && apt-get install -y --no-install-recommends clang-22 llvm-22-dev lld-22 \
  && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh
WORKDIR /src
ENV LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm
ENV CC=clang-22
ENV CXX=clang++-22
ENV LI_LLVM_MAJOR=22
ENV CI=true
