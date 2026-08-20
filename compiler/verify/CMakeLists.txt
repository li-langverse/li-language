add_library(li_verify STATIC vc_summary.cpp vc_emit.cpp vc_emit_lean.cpp vc_witness.cpp
  vc_prove.cpp vc_li_prover.cpp call_requires.cpp)
target_include_directories(li_verify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_verify PUBLIC li_ast li_mir li_types)
