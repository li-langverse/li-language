#include "li/workspace_check.hpp"

#include "li/check_cache.hpp"
#include "li/resource_options.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace li {
namespace {

std::vector<std::string> workspace_members(const std::filesystem::path& workspace_toml) {
  std::ifstream in(workspace_toml);
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();
  std::vector<std::string> members;
  std::size_t members_pos = text.find("members =");
  if (members_pos == std::string::npos) {
    members_pos = text.find("members=");
  }
  if (members_pos == std::string::npos) {
    return members;
  }
  const std::size_t open = text.find('[', members_pos);
  if (open == std::string::npos) {
    return members;
  }
  int depth = 0;
  std::size_t close = std::string::npos;
  for (std::size_t i = open; i < text.size(); ++i) {
    if (text[i] == '[') {
      ++depth;
    } else if (text[i] == ']') {
      --depth;
      if (depth == 0) {
        close = i;
        break;
      }
    }
  }
  if (close == std::string::npos || close <= open) {
    return members;
  }
  for (std::size_t i = open + 1; i < close; ++i) {
    if (text[i] != '"') {
      continue;
    }
    const std::size_t end = text.find('"', i + 1);
    if (end == std::string::npos || end >= close) {
      break;
    }
    members.push_back(text.substr(i + 1, end - i - 1));
    i = end;
  }
  return members;
}

std::filesystem::path default_workspace_toml() {
  return std::filesystem::current_path() / "packages" / "li.toml";
}

std::filesystem::path repo_root_for_workspace(const std::filesystem::path& workspace_toml) {
  const auto parent = workspace_toml.parent_path();
  if (parent.filename() == "packages") {
    return parent.parent_path();
  }
  return parent;
}

std::optional<std::filesystem::path> member_check_entry(const std::filesystem::path& repo_root,
                                                        const std::string& member) {
  const auto pkg = repo_root / "packages" / member;
  const auto smoke = pkg / "li-tests" / "smoke" / "builds.li";
  if (std::filesystem::exists(smoke)) {
    return smoke;
  }
  const auto lib = pkg / "src" / "lib.li";
  if (std::filesystem::exists(lib)) {
    return lib;
  }
  return std::nullopt;
}

bool is_workspace_flag(std::string_view arg) {
  return arg == "--workspace" || arg.rfind("--workspace=", 0) == 0;
}

#if !defined(_WIN32)
int spawn_lic_check(const char* lic_exe, const std::vector<std::string>& args) {
  const pid_t pid = fork();
  if (pid < 0) {
    return 1;
  }
  if (pid == 0) {
    const int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      close(devnull);
    }
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(lic_exe));
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execv(lic_exe, argv.data());
    _exit(1);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return 1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
}
#endif

}  // namespace

int lic_workspace_check_main(int argc, char** argv, const char* lic_executable,
                             const CheckCacheOptions& cache) {
  std::filesystem::path workspace_toml = default_workspace_toml();
  bool deny_warnings = false;
  ResourceOptions resources;
  resources.job_memory_mb = 128;
  CheckCacheOptions child_cache = cache;

  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (is_workspace_flag(arg)) {
      if (arg.rfind("--workspace=", 0) == 0) {
        workspace_toml = std::filesystem::path(std::string(arg.substr(12)));
      }
      continue;
    }
    if (arg == "--deny-warnings") {
      deny_warnings = true;
      continue;
    }
    if (apply_check_cache_flag(arg, child_cache)) {
      continue;
    }
    if (apply_resource_flag(arg, resources)) {
      continue;
    }
    if (!arg.empty() && arg[0] != '-') {
      workspace_toml = std::filesystem::path(std::string(arg));
      continue;
    }
    std::cerr << "usage: lic check --workspace [li.toml] [--jobs=N] [--max-memory=MB] "
                 "[--cache-dir=DIR] [--deny-warnings]\n";
    return 1;
  }

  finalize_resource_options(resources);
  const unsigned jobs = resources.effective_jobs(128);
  const auto repo_root = repo_root_for_workspace(workspace_toml);
  const auto members = workspace_members(workspace_toml);
  if (members.empty()) {
    std::cerr << "lic check --workspace: no members in " << workspace_toml << "\n";
    return 1;
  }

  int fail = 0;
  unsigned active = 0;
  unsigned checked = 0;

  auto make_child_args = [&](const std::filesystem::path& entry) {
    std::vector<std::string> child_args = {"check", entry.string(), "--format=json"};
    if (deny_warnings) {
      child_args.push_back("--deny-warnings");
    }
    if (!child_cache.enabled) {
      child_args.push_back("--no-cache");
    } else if (!child_cache.cache_dir.empty()) {
      child_args.push_back("--cache-dir=" + child_cache.cache_dir.string());
    }
    if (child_cache.max_mb > 0) {
      child_args.push_back("--cache-max-mb=" + std::to_string(child_cache.max_mb));
    }
    return child_args;
  };

  for (const auto& member : members) {
    if (member == "li-demo") {
      continue;
    }
    const auto entry = member_check_entry(repo_root, member);
    if (!entry) {
      continue;
    }
    const auto child_args = make_child_args(*entry);
#if defined(_WIN32)
    std::ostringstream cmd;
    cmd << '"' << lic_executable << '"';
    for (const auto& a : child_args) {
      cmd << ' ' << '"' << a << '"';
    }
    if (std::system(cmd.str().c_str()) != 0) {
      fail = 1;
    }
#else
    if (jobs <= 1) {
      if (spawn_lic_check(lic_executable, child_args) != 0) {
        fail = 1;
      }
    } else {
      while (active >= jobs) {
        int status = 0;
        if (waitpid(-1, &status, 0) > 0) {
          --active;
          if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fail = 1;
          }
        }
      }
      std::vector<std::string> arg_storage = child_args;
      const pid_t pid = fork();
      if (pid == 0) {
        const int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
          dup2(devnull, STDOUT_FILENO);
          close(devnull);
        }
        std::vector<char*> cargv;
        cargv.push_back(const_cast<char*>(lic_executable));
        for (auto& a : arg_storage) {
          cargv.push_back(a.data());
        }
        cargv.push_back(nullptr);
        execv(lic_executable, cargv.data());
        _exit(1);
      }
      if (pid > 0) {
        ++active;
      } else {
        fail = 1;
      }
    }
#endif
    ++checked;
  }

#if !defined(_WIN32)
  if (jobs > 1) {
    while (active > 0) {
      int status = 0;
      if (waitpid(-1, &status, 0) > 0) {
        --active;
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
          fail = 1;
        }
      }
    }
  }
#endif

  if (checked == 0) {
    std::cerr << "lic check --workspace: no checkable members under " << repo_root / "packages"
              << "\n";
    return 1;
  }
  if (fail == 0) {
    std::cout << "lic check --workspace: ok (members=" << checked << ", jobs=" << jobs << ")\n";
  }
  return fail;
}

}  // namespace li
