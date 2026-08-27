# Core compiler runtime: language primitives only. Package runtimes are
# separate targets so `lic` never embeds HTTPD/LIG/domain policy.
add_library(li_rt_core STATIC li_rt.c)
target_link_libraries(li_rt_core PRIVATE dl)
target_include_directories(li_rt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Compatibility aggregate for existing package builds. New package manifests
# should link the narrow target they require instead of this bundle.
add_library(li_rt STATIC li_rt.c li_rt_httpd.c li_rt_log.c li_rt_net.c li_rt_tls.c li_rt_h2.c)
target_link_libraries(li_rt PRIVATE dl)
target_include_directories(li_rt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(li_rt_httpd STATIC li_rt_httpd.c)
target_link_libraries(li_rt_httpd PUBLIC li_rt_core)
target_include_directories(li_rt_httpd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(li_rt_lig STATIC li_rt_lig.c)
target_link_libraries(li_rt_lig PUBLIC li_rt_core)
target_include_directories(li_rt_lig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
