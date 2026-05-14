add_library(li_mir STATIC mir.cpp lower.cpp)
target_include_directories(li_mir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_mir PUBLIC li_types li_ast)

