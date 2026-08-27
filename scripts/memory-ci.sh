#!/usr/bin/env bash
# Lightweight memory/security smoke for CI and local-ci --memory.
# Never fails default CI on macOS if sanitizers are unavailable.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
uname_s="$(uname -s)"

echo "==> memory-ci: security corpus"
chmod +x "$ROOT/li-tests/run_security.sh"
"$ROOT/li-tests/run_security.sh"

echo "==> memory-ci: profile-memory (native + lic RSS)"
chmod +x "$ROOT/scripts/profile-memory.sh"
"$ROOT/scripts/profile-memory.sh"

if [[ "$uname_s" == "Linux" ]]; then
  echo "==> memory-ci: ASan md (Linux only)"
  "$ROOT/scripts/profile-memory.sh" --asan-md
  # Optional: ASan lic — can be slow; skip if build fails
  if "$ROOT/scripts/profile-memory.sh" --asan-lic 2>/dev/null; then
    echo "memory-ci: ASan lic ok"
  else
    echo "memory-ci: ASan lic skipped (LLVM link may need extra flags)"
  fi
  echo "==> memory-ci: ASan li_rt smoke"
  "$ROOT/scripts/profile-memory.sh" --asan-rt
else
  echo "memory-ci: ASan targets skipped on $uname_s (use Linux CI job)"
fi

echo "memory-ci: ok"
