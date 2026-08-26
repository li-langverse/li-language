#pragma once

#include "li/ast.hpp"
#include "li/mir.hpp"

#include <string>

namespace li {

/// Reject MIR where pointer-width extern returns or CallExtern results would truncate.
bool verify_mir_extern_abi(const Module& module, const MirModule& mir, std::string* error);

}  // namespace li
