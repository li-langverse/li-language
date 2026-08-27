#include "li/import_resolve.hpp"

#include "li/parser.hpp"
#include "li/prelude.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

namespace li {
namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::filesystem::path repo_root() {
  if (const char* root = std::getenv("LI_REPO_ROOT")) {
    return std::filesystem::path(root);
  }
  return std::filesystem::current_path();
}

std::filesystem::path std_root() {
  const std::filesystem::path root = repo_root();
  if (const char* std_path = std::getenv("LI_STD_PATH")) {
    return std::filesystem::path(std_path);
  }
  if (std::filesystem::exists(root / "std")) {
    return root / "std";
  }
  return std::filesystem::path("std");
}

std::string kebab_to_snake(std::string s) {
  for (char& c : s) {
    if (c == '-') {
      c = '_';
    }
  }
  return s;
}

std::filesystem::path std_module_to_path(const std::string& module) {
  std::string rest = module;
  if (rest.rfind("std.", 0) == 0) {
    rest = rest.substr(4);
  } else if (rest == "std") {
    rest.clear();
  }
  std::filesystem::path p = std_root();
  std::size_t i = 0;
  std::string last;
  while (i <= rest.size()) {
    const std::size_t dot = rest.find('.', i);
    const std::string seg =
        dot == std::string::npos ? rest.substr(i) : rest.substr(i, dot - i);
    if (!seg.empty()) {
      last = seg;
      p /= seg;
    }
    if (dot == std::string::npos) {
      break;
    }
    i = dot + 1;
  }
  if (!last.empty()) {
    /* Multi-segment (e.g. physics.relativity): .../physics/relativity → .../relativity.li */
    if (rest.find('.') != std::string::npos) {
      p = p.parent_path() / (last + ".li");
    } else {
      /* Single-segment (e.g. bytes, csv): .../bytes → .../bytes/bytes.li */
      p = p / (last + ".li");
    }
  } else {
    p /= "lib.li";
  }
  return p;
}

bool is_workspace_toml(const std::filesystem::path& toml) {
  const std::string text = read_file(toml);
  return text.find("[workspace]") != std::string::npos;
}

std::optional<std::filesystem::path> find_workspace_toml(
    const std::filesystem::path& from_file) {
  const std::filesystem::path file_abs =
      from_file.is_absolute() ? from_file : std::filesystem::absolute(from_file);
  std::filesystem::path dir = file_abs.parent_path();
  for (int depth = 0; depth < 12 && !dir.empty(); ++depth) {
    const auto nested = dir / "packages" / "li.toml";
    if (std::filesystem::exists(nested)) {
      return nested;
    }
    const auto here = dir / "li.toml";
    if (std::filesystem::exists(here) && is_workspace_toml(here)) {
      return here;
    }
    dir = dir.parent_path();
  }
  const auto root = repo_root();
  const auto root_ws = root / "packages" / "li.toml";
  if (std::filesystem::exists(root_ws)) {
    return root_ws;
  }
  return std::nullopt;
}

std::vector<std::string> parse_workspace_members(const std::filesystem::path& toml) {
  std::vector<std::string> members;
  const std::string text = read_file(toml);
  const std::string key = "members";
  const std::size_t pos = text.find(key);
  if (pos == std::string::npos) {
    return members;
  }
  const std::size_t eq = text.find('=', pos);
  if (eq == std::string::npos) {
    return members;
  }
  const std::size_t bracket = text.find('[', eq);
  if (bracket == std::string::npos) {
    return members;
  }
  int depth = 0;
  std::size_t end = std::string::npos;
  for (std::size_t i = bracket; i < text.size(); ++i) {
    if (text[i] == '[') {
      ++depth;
    } else if (text[i] == ']') {
      --depth;
      if (depth == 0) {
        end = i;
        break;
      }
    }
  }
  if (end == std::string::npos) {
    return members;
  }
  std::string slice = text.substr(bracket + 1, end - bracket - 1);
  std::size_t i = 0;
  while (i < slice.size()) {
    const std::size_t q1 = slice.find('"', i);
    if (q1 == std::string::npos) {
      break;
    }
    const std::size_t q2 = slice.find('"', q1 + 1);
    if (q2 == std::string::npos) {
      break;
    }
    members.push_back(slice.substr(q1 + 1, q2 - q1 - 1));
    i = q2 + 1;
  }
  return members;
}

std::optional<std::filesystem::path> find_package_toml(const std::filesystem::path& from_file) {
  std::filesystem::path dir = from_file.parent_path();
  for (int depth = 0; depth < 12 && !dir.empty(); ++depth) {
    const auto toml = dir / "li.toml";
    if (std::filesystem::exists(toml)) {
      return toml;
    }
    dir = dir.parent_path();
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> path_dependency_entry(const std::filesystem::path& package_toml,
                                                         const std::string& module) {
  const std::string text = read_file(package_toml);
  const std::size_t deps = text.find("[dependencies]");
  if (deps == std::string::npos) {
    return std::nullopt;
  }
  const std::string tail = text.substr(deps);
  std::size_t pos = 0;
  while (pos < tail.size()) {
    const std::size_t line_end = tail.find('\n', pos);
    const std::string line = tail.substr(pos, line_end == std::string::npos ? std::string::npos
                                                                          : line_end - pos);
    pos = line_end == std::string::npos ? tail.size() : line_end + 1;
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    std::string dep_name = line.substr(0, eq);
    while (!dep_name.empty() && (dep_name.back() == ' ' || dep_name.back() == '\t')) {
      dep_name.pop_back();
    }
    if (kebab_to_snake(dep_name) != module && dep_name != module) {
      continue;
    }
    const std::size_t path_key = line.find("path");
    if (path_key == std::string::npos) {
      continue;
    }
    const std::size_t q1 = line.find('"', path_key);
    const std::size_t q2 = line.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) {
      continue;
    }
    const std::string rel = line.substr(q1 + 1, q2 - q1 - 1);
    const std::filesystem::path lib = (package_toml.parent_path() / rel / "src" / "lib.li");
    if (std::filesystem::exists(lib)) {
      return lib;
    }
    const std::filesystem::path vendor =
        package_toml.parent_path() / ".li" / "vendor" / dep_name / "src" / "lib.li";
    if (std::filesystem::exists(vendor)) {
      return vendor;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> same_package_entry(const std::filesystem::path& package_toml,
                                                        const std::string& module) {
  const std::string text = read_file(package_toml);
  const std::size_t name_key = text.find("name");
  if (name_key == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t q1 = text.find('"', name_key);
  const std::size_t q2 = text.find('"', q1 + 1);
  if (q1 == std::string::npos || q2 == std::string::npos) {
    return std::nullopt;
  }
  const std::string pkg_name = text.substr(q1 + 1, q2 - q1 - 1);
  if (kebab_to_snake(pkg_name) != module && pkg_name != module) {
    return std::nullopt;
  }
  const std::filesystem::path lib = package_toml.parent_path() / "src" / "lib.li";
  if (std::filesystem::exists(lib)) {
    return lib;
  }
  return std::nullopt;
}

std::optional<std::string> parse_metadata_import_name(const std::filesystem::path& package_toml) {
  const std::string text = read_file(package_toml);
  const std::size_t pos = text.find("import_name");
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t q1 = text.find('"', pos);
  const std::size_t q2 = text.find('"', q1 + 1);
  if (q1 == std::string::npos || q2 == std::string::npos) {
    return std::nullopt;
  }
  return text.substr(q1 + 1, q2 - q1 - 1);
}

std::optional<std::filesystem::path> workspace_package_entry(
    const std::filesystem::path& workspace_toml, const std::string& module) {
  const std::filesystem::path packages_dir = workspace_toml.parent_path();
  for (const std::string& member : parse_workspace_members(workspace_toml)) {
    const auto pkg_toml = packages_dir / member / "li.toml";
    if (std::filesystem::exists(pkg_toml)) {
      if (const auto alias = parse_metadata_import_name(pkg_toml)) {
        if (*alias == module) {
          const std::filesystem::path lib = packages_dir / member / "src" / "lib.li";
          if (std::filesystem::exists(lib)) {
            return lib;
          }
        }
      }
    }
    const std::string snake = kebab_to_snake(member);
    if (module != snake && module != member) {
      continue;
    }
    const std::filesystem::path lib = packages_dir / member / "src" / "lib.li";
    if (std::filesystem::exists(lib)) {
      return lib;
    }
  }
  return std::nullopt;
}

/// Map ergonomic imports (physics.relativity) to std tree paths (std/physics/relativity.li).
std::optional<std::string> easy_std_module(const std::string& module) {
  if (module.rfind("std.", 0) == 0 || module == "std") {
    return module;
  }
  if (module.rfind("physics.", 0) == 0) {
    return "std." + module;
  }
  if (module == "physics") {
    return "std.physics.core";
  }
  if (module.rfind("math.", 0) == 0) {
    return "std." + module;
  }
  if (module == "math") {
    return "std.math";
  }
  if (module.rfind("ui.", 0) == 0 || module == "ui") {
    return module == "ui" ? "std.ui" : "std." + module;
  }
  if (module.rfind("scene.", 0) == 0 || module == "scene") {
    return module == "scene" ? "std.scene" : "std." + module;
  }
  if (module == "io") {
    return "std.io";
  }
  if (module.rfind("io.", 0) == 0) {
    return "std." + module;
  }
  if (module == "csv") {
    return "std.csv";
  }
  if (module.rfind("csv.", 0) == 0) {
    return "std." + module;
  }
  return std::nullopt;
}

std::vector<std::filesystem::path> local_module_candidates(const std::filesystem::path& importer,
                                                         const std::string& module) {
  std::vector<std::filesystem::path> out;
  const std::filesystem::path dir = importer.parent_path();
  const std::size_t last_dot = module.find_last_of('.');
  const std::string tail = last_dot == std::string::npos ? module : module.substr(last_dot + 1);

  std::filesystem::path dotted = dir;
  std::size_t i = 0;
  while (i <= module.size()) {
    const std::size_t dot = module.find('.', i);
    const std::string seg =
        dot == std::string::npos ? module.substr(i) : module.substr(i, dot - i);
    if (!seg.empty()) {
      dotted /= seg;
    }
    if (dot == std::string::npos) {
      break;
    }
    i = dot + 1;
  }

  out.push_back(dir / (module + ".li"));
  out.push_back(dir / (tail + ".li"));
  out.push_back(dotted.parent_path() / (tail + ".li"));
  out.push_back(dotted / (tail + ".li"));
  out.push_back(dir / tail / (tail + ".li"));
  return out;
}

std::optional<std::filesystem::path> resolve_module_path(const std::string& module,
                                                         const std::filesystem::path& importer) {
  const std::filesystem::path importer_abs =
      importer.is_absolute() ? importer : std::filesystem::absolute(importer);
  // Workspace packages (import_name in li.toml) win over std facades for the same path.
  if (const auto ws = find_workspace_toml(importer_abs)) {
    if (auto p = workspace_package_entry(*ws, module)) {
      return p;
    }
  }

  if (const auto std_mod = easy_std_module(module)) {
    const std::filesystem::path p = std_module_to_path(*std_mod);
    if (std::filesystem::exists(p)) {
      return p;
    }
  }

  if (module.rfind("std.", 0) == 0 || module == "std") {
    const std::filesystem::path p = std_module_to_path(module);
    if (std::filesystem::exists(p)) {
      return p;
    }
    return std::nullopt;
  }

  for (const auto& c : local_module_candidates(importer_abs, module)) {
    if (std::filesystem::exists(c)) {
      return c;
    }
  }

  if (const auto pkg_toml = find_package_toml(importer_abs)) {
    if (auto p = same_package_entry(*pkg_toml, module)) {
      return p;
    }
    if (auto p = path_dependency_entry(*pkg_toml, module)) {
      return p;
    }
  }

  return std::nullopt;
}

void merge_module(Module& into, Module&& from) {
  for (auto& t : from.types) {
    into.types.push_back(std::move(t));
  }
  for (auto& p : from.procs) {
    if (p.visibility == Visibility::Public) {
      into.procs.push_back(std::move(p));
    }
  }
}

bool load_module_recursive(const std::filesystem::path& mod_path, Module& out,
                         const std::string& file_path, DiagnosticBag& diags,
                         std::set<std::string>& loading, std::set<std::string>& loaded) {
  const std::string key = mod_path.lexically_normal().string();
  if (loaded.count(key)) {
    return true;
  }
  if (loading.count(key)) {
    SourceLoc loc{file_path, 1, 1, 0};
    diags.error(loc, "import_cycle: " + mod_path.string());
    return false;
  }
  loading.insert(key);

  const std::string source = read_file(mod_path);
  auto parsed = parse_module(source, mod_path.string());
  for (const auto& d : parsed.diagnostics.items()) {
    diags.error(d.loc, d.message);
  }
  if (!parsed.module) {
    loading.erase(key);
    return diags.empty();
  }

  check_stdlib_seal(*parsed.module, mod_path.string(), diags);
  if (!diags.empty()) {
    loading.erase(key);
    return false;
  }

  Module imported = std::move(*parsed.module);
  const auto nested = imported.imports;
  for (const auto& imp : nested) {
    const auto nested_path = resolve_module_path(imp.module, mod_path);
    if (!nested_path) {
      SourceLoc loc{mod_path.string(), 1, 1, imp.span.start};
      diags.error(loc, "import_resolve: module not found: " + imp.module);
      continue;
    }
    if (!load_module_recursive(*nested_path, imported, mod_path.string(), diags, loading,
                               loaded)) {
      loading.erase(key);
      return false;
    }
  }

  merge_module(out, std::move(imported));
  loading.erase(key);
  loaded.insert(key);
  return true;
}

}  // namespace

bool resolve_imports(Module& out, const std::string& file_path, DiagnosticBag& diags) {
  const auto imports = out.imports;
  std::set<std::string> loading;
  std::set<std::string> loaded;

  for (const auto& imp : imports) {
    const std::filesystem::path importer(file_path);
    const auto mod_path = resolve_module_path(imp.module, importer);
    if (!mod_path) {
      SourceLoc loc{file_path, 1, 1, imp.span.start};
      diags.error(loc, "import_resolve: module not found: " + imp.module);
      continue;
    }
    if (!load_module_recursive(*mod_path, out, file_path, diags, loading, loaded)) {
      continue;
    }
  }
  // Warnings from earlier frontend passes (e.g. policy W0403) must not fail
  // import resolution — only errors do.
  return !diags.has_errors();
}

}  // namespace li
