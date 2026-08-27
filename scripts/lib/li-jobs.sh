# Host parallelism defaults for Li tooling (8p).
# shellcheck shell=bash

li_host_jobs() {
  if [[ -n "${LI_BUILD_JOBS:-}" ]]; then
    echo "$LI_BUILD_JOBS"
    return
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null || echo 4
  elif command -v nproc >/dev/null 2>&1; then
    nproc
  else
    echo 4
  fi
}

li_test_jobs() {
  if [[ -n "${LI_TEST_JOBS:-}" ]]; then
    echo "$LI_TEST_JOBS"
    return
  fi
  if [[ "${CI:-}" == "true" ]]; then
    li_host_jobs
    return
  fi
  echo 1
}

li_workspace_jobs() {
  if [[ -n "${LI_WORKSPACE_JOBS:-}" ]]; then
    echo "$LI_WORKSPACE_JOBS"
    return
  fi
  li_test_jobs
}
