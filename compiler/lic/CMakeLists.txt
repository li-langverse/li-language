add_executable(lic main.cpp)
target_compile_definitions(lic PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_link_libraries(lic PRIVATE li_ast li_codegen li_diagnostics li_lexer li_parser li_types li_mir li_verify)

# vc_emit.hpp (AutoVC Lean emission) lives in the verify library's include dir.
target_include_directories(lic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../verify/include)
