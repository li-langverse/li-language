add_library(li_analyze STATIC advisory.cpp)
target_include_directories(li_analyze PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(li_analyze PUBLIC li_ast li_config li_diagnostics)
