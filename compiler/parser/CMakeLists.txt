add_library(li_parser STATIC parser.cpp)
target_include_directories(li_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_parser PUBLIC li_lexer li_ast li_diagnostics)
