add_executable(lic main.cpp)
target_compile_definitions(lic PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_link_libraries(lic PRIVATE li_codegen li_diagnostics li_parser li_types)
