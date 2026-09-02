# vc_li_prover.cpp serves the removed `lic verify` command and needs the
# pre-squash AST (TheoremDecl); restore it only if verify returns.
add_library(li_verify STATIC vc_summary.cpp vc_emit.cpp vc_emit_lean.cpp vc_witness.cpp
  vc_prove.cpp call_requires.cpp)
target_include_directories(li_verify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(li_verify PUBLIC li_ast li_mir li_types)
