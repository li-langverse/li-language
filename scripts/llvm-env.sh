#!/usr/bin/env bash
# LLVM toolchain pin for Li (sourced by build.sh, local-ci.sh, CI helpers).
# Override: export LI_LLVM_MAJOR=22 LLVM_DIR=... before build.
LI_LLVM_MAJOR="${LI_LLVM_MAJOR:-22}"

li_llvm_cmake_dir() {
  local v="$1"
  local candidates=(
    "/usr/lib/llvm-${v}/lib/cmake/llvm"
    "/opt/homebrew/opt/llvm@${v}/lib/cmake/llvm"
    "/usr/local/opt/llvm@${v}/lib/cmake/llvm"
  )
  local d
  for d in "${candidates[@]}"; do
    if [[ -f "$d/LLVMConfig.cmake" ]]; then
      echo "$d"
      return 0
    fi
  done
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
      for d in \
        "/c/Program Files/LLVM/lib/cmake/llvm" \
        "/c/Program Files/LLVM/lib/cmake/llvm-${v}"; do
        if [[ -f "$d/LLVMConfig.cmake" ]]; then
          echo "$d"
          return 0
        fi
      done
      ;;
  esac
  return 1
}

li_detect_llvm_dir() {
  if [[ -n "${LLVM_DIR:-}" && -f "${LLVM_DIR}/LLVMConfig.cmake" ]]; then
    return 0
  fi
  local v dir
  for v in "$LI_LLVM_MAJOR" 21 20; do
    dir="$(li_llvm_cmake_dir "$v" || true)"
    if [[ -n "$dir" ]]; then
      export LLVM_DIR="$dir"
      export LI_LLVM_MAJOR="$v"
      return 0
    fi
  done
  return 1
}

li_detect_compilers() {
  local v="${LI_LLVM_MAJOR:-22}"
  if [[ -n "${CC:-}" && -n "${CXX:-}" ]]; then
    return 0
  fi
  if [[ "$(uname -s)" == "Darwin" ]]; then
    export CC="${CC:-clang}"
    export CXX="${CXX:-clang++}"
    return 0
  fi
  if command -v "clang-${v}" >/dev/null 2>&1; then
    export CC="clang-${v}"
    export CXX="clang++-${v}"
    return 0
  fi
  export CC="${CC:-clang}"
  export CXX="${CXX:-clang++}"
}

li_llvm_install_hint() {
  local v="${LI_LLVM_MAJOR:-22}"
  echo "LLVM ${v} required. Examples:" >&2
  echo "  Debian bookworm: wget https://apt.llvm.org/llvm.sh && sudo ./llvm.sh ${v}" >&2
  echo "  Ubuntu 24.04+:   sudo apt install clang-${v} llvm-${v}-dev" >&2
  echo "  macOS:           brew install llvm@${v}" >&2
  echo "  export LLVM_DIR=\$(brew --prefix llvm@${v})/lib/cmake/llvm" >&2
}
