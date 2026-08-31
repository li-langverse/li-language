cmake_minimum_required(VERSION 3.20)
project(li LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers (Linux CI / local debug). macOS + Homebrew LLVM link is often brittle.
# Usage: cmake -B build-asan -DLI_SANITIZE=address -DLLVM_DIR=...
set(LI_SANITIZE "" CACHE STRING "Sanitizer: address, undefined, thread, or empty (off)")
if(LI_SANITIZE)
  add_compile_options(-fsanitize=${LI_SANITIZE} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${LI_SANITIZE})
  message(STATUS "LI_SANITIZE=${LI_SANITIZE}")
endif()

option(LI_BUILD_FUZZ "Build libFuzzer parser target (Clang + -fsanitize=fuzzer)" OFF)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" LI_VERSION_RAW)
string(STRIP "${LI_VERSION_RAW}" LI_VERSION_STRING)

# LLVM 22 (org pin) — set -DLLVM_DIR=/path/to/lib/cmake/llvm when not on PATH
set(LI_LLVM_VERSION_MAJOR 22 CACHE STRING "Required LLVM major version")
find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL ${LI_LLVM_VERSION_MAJOR})
  message(FATAL_ERROR "LLVM ${LI_LLVM_VERSION_MAJOR} required, found ${LLVM_PACKAGE_VERSION}")
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

add_subdirectory(compiler)
add_subdirectory(runtime)
