add_executable(lic main.cpp)
target_compile_definitions(lic PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_link_libraries(lic PRIVATE li_ast li_codegen li_diagnostics li_lexer li_parser li_types li_mir)
