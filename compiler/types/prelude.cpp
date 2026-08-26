#include "li/prelude.hpp"

#include "li/error_codes.hpp"
#include "li/numeric_types.hpp"

#include <set>
#include <string>
#include <string_view>

namespace li {
namespace {

bool in_set(std::string_view name, std::initializer_list<const char*> names) {
  for (const char* n : names) {
    if (n == nullptr) {
      break;
    }
    if (name == n) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool is_prelude_type_name(const std::string_view name) {
  if (is_numeric_scalar_type_name(name)) {
    return true;
  }
  return in_set(name, {"bool", "str", "binary", "Binary", "unit", "list", "dict", "tuple",
                       "Option", "simd", "bytes", "stringview", "ptr", nullptr});
}

bool is_prelude_proc_name(const std::string_view name) {
  return in_set(name, {"print", "sum", "dot", "norm", "axpy", "disjoint_elem", "disjoint_row",
                       "disjoint_slice", "row_ok", nullptr});
}

bool is_reserved_decorator_name(const std::string_view name) {
  return in_set(name, {"cpu",          "gpu",          "tpu",          "user_defined",
                       "parallel",     "vectorized",   "async",        "serial",
                       "no_vectorize", nullptr});
}

bool is_std_module_symbol(const std::string_view name) {
  // Sync with std/**/*.li top-level symbols (see scripts/gen-stdlib-manifest.sh).
  return in_set(name, {"__execution_decorators_doc", "Bytes", "StringView", "Reader",
                       "Writer", "bytes_len", "bytes_slice", "bytes_append", "bytes_byte_at",
                       "bytes_push_byte", "stringview_len", "stringview_byte_at", "reader_new",
                       "reader_remaining", "reader_read_byte", "reader_read_chunk", "writer_new",
                       "writer_push_byte", "writer_len", nullptr});
}

void check_duplicate_definitions(const Module& module, const std::string& file,
                                 DiagnosticBag& diags) {
  std::set<std::string> seen_procs;
  std::set<std::string> seen_types;
  auto report = [&](const Span& span, const std::string& msg) {
    diags.error(SourceLoc{file, 1, 1, span.start}, msg);
  };
  for (const auto& alias : module.types) {
    if (seen_types.count(alias.name)) {
      report(alias.span, "duplicate_definition: " + alias.name);
    }
    seen_types.insert(alias.name);
  }
  for (const auto& proc : module.procs) {
    if (seen_procs.count(proc.name)) {
      report(proc.span, "duplicate_definition: " + proc.name);
    }
    seen_procs.insert(proc.name);
  }
  // Theorems/axioms/lemmas share the top-level symbol space with procs.
  for (const auto& thm : module.theorems) {
    if (seen_procs.count(thm.name)) {
      report(thm.span, "duplicate_definition: " + thm.name);
    }
    seen_procs.insert(thm.name);
  }
}

void check_stdlib_seal(const Module& module, const std::string& file,
                       DiagnosticBag& diags) {
  auto report_shadow = [&](const Span& span, const std::string& name) {
    diag_error(diags, SourceLoc{file, 1, 1, span.start}, ErrorCode::E0330,
               "The name `" + name +
                   "` is reserved for the standard library — pick a different name "
                   "(stdlib_symbol_shadow: " +
                   name + ").",
               "Rename this symbol, or move it into a module under `std/` if you are extending "
               "the prelude.");
  };
  const bool defining_std =
      file.find("/std/") != std::string::npos || file.find("\\std\\") != std::string::npos ||
      (file.size() >= 4 && file.compare(0, 4, "std/") == 0);

  for (const auto& alias : module.types) {
    if (is_prelude_type_name(alias.name)) {
      report_shadow(alias.span, alias.name);
    }
    if (!defining_std && is_std_module_symbol(alias.name)) {
      report_shadow(alias.span, alias.name);
    }
  }

  for (const auto& proc : module.procs) {
    if (is_prelude_proc_name(proc.name)) {
      report_shadow(proc.span, proc.name);
    }
    if (!defining_std && is_std_module_symbol(proc.name)) {
      report_shadow(proc.span, proc.name);
    }
    if (is_prelude_type_name(proc.name)) {
      report_shadow(proc.span, proc.name);
    }
  }
}

}  // namespace li
