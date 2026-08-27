#!/usr/bin/env bash
# Backward-compatible wrapper — delegates to li-tests.
exec "$(cd "$(dirname "$0")/.." && pwd)/li-tests/run_all.sh" race_shared_memory "$@"
