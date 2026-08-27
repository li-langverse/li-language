#pragma once

#include "li/ast.hpp"
#include "li/vc_summary.hpp"

#include <string>

namespace li {

bool write_vcs_json(const Module& module, const std::string& path, std::string* err);
bool write_vcs_lean(const Module& module, const std::string& path, std::string* err,
                    std::size_t* native_closed = nullptr);

}  // namespace li
