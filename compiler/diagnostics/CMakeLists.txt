add_library(li_diagnostics STATIC diagnostics.cpp)
target_include_directories(li_diagnostics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
