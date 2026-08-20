add_executable(lic main.cpp check_cmd.cpp workspace_check.cpp)
target_compile_definitions(lic PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_include_directories(lic PRIVATE "${CMAKE_SOURCE_DIR}/runtime"
                                           "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lic PRIVATE li_cache li_common li_analyze li_codegen li_config li_diagnostics
                                   li_parser li_types li_verify li_mir li_rt)

# Standard `cmake --install build` path. The binary is self-contained, so only
# the executable needs to be installed.
include(GNUInstallDirs)
install(TARGETS lic RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
