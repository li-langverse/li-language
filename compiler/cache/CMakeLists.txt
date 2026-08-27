add_library(li_cache STATIC check_cache.cpp)
target_compile_definitions(li_cache PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_include_directories(li_cache PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(li_cache PUBLIC li_config li_diagnostics)
