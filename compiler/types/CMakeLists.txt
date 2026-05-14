add_library(li_types STATIC types.cpp typecheck.cpp policy.cpp borrowck.cpp)
target_include_directories(li_types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_types PUBLIC li_ast li_diagnostics)
