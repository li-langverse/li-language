#pragma once

#include "li/check_cache.hpp"

namespace li {

int lic_workspace_check_main(int argc, char** argv, const char* lic_executable,
                             const CheckCacheOptions& cache);

}  // namespace li
