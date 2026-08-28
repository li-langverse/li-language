add_library(li_config STATIC check_config.cpp)
target_include_directories(li_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(li_config PUBLIC li_diagnostics)
