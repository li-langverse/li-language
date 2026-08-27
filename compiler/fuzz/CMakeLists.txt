# Optional libFuzzer target (Clang). Not part of default CI.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "LI_BUILD_FUZZ requires Clang; skipping parse_fuzz")
  return()
endif()

add_executable(parse_fuzz parse_fuzz.cpp)
target_link_libraries(parse_fuzz PRIVATE li_parser)
target_compile_options(parse_fuzz PRIVATE -fsanitize=fuzzer-no-link)
target_link_options(parse_fuzz PRIVATE -fsanitize=fuzzer)

add_executable(check_fuzz check_fuzz.cpp)
target_link_libraries(check_fuzz PRIVATE li_parser li_types)
target_compile_options(check_fuzz PRIVATE -fsanitize=fuzzer-no-link)
target_link_options(check_fuzz PRIVATE -fsanitize=fuzzer)
