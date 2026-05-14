cmake_minimum_required(VERSION 3.20)
project(li LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" LI_VERSION_RAW)
string(STRIP "${LI_VERSION_RAW}" LI_VERSION_STRING)

# LLVM 18 — set -DLLVM_DIR=/path/to/lib/cmake/llvm when not on PATH
find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 18)
  message(FATAL_ERROR "LLVM 18 required, found ${LLVM_PACKAGE_VERSION}")
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

add_subdirectory(compiler)
add_subdirectory(runtime)
