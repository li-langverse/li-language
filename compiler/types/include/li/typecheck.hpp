#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace li {

struct TypecheckResult {
  bool ok = false;
  DiagnosticBag diagnostics;
};

// Typecheck `module`. `main_proc_count` limits which proc bodies are checked
// to the first N (the main file's own procs); all procs (including imported
// ones) remain in scope so call signatures resolve, but imported proc bodies
// are lowered without a typecheck pass and must not be checked here. Pass
// module.procs.size() to check everything (single-module callers).
TypecheckResult typecheck_module(const Module& module, std::size_t main_proc_count);

}  // namespace li
