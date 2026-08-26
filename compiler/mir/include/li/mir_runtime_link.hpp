#pragma once

#include "li/mir.hpp"

#include <string_view>

namespace li {

/** Update link flags from an extern/runtime callee name (CallExtern). */
void mir_note_runtime_callee(std::string_view callee, MirModule& mir);

/** httpd objects depend on net (li_rt_httpd.c includes li_rt_net.h). */
void mir_finalize_runtime_link_needs(MirModule& mir);

/** Scan lowered MIR for CallExtern callees. */
void mir_collect_runtime_link_needs(const MirModule& mir, MirModule& out_flags);

}  // namespace li
