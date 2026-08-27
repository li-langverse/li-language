add_library(li_lexer STATIC lexer.cpp token.cpp)
target_include_directories(li_lexer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_lexer PUBLIC li_diagnostics)
