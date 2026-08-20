#!/usr/bin/env bash
# Nightly CI: same pipeline as scripts/ci.sh, but the per-package standalone
# carve-out gate runs the FULL workspace sweep (every member of packages/
# installs, builds and runs outside the monorepo) instead of the 20-package
# subset. Invoked by the nightly job (cron / schedule):
#
#   scripts/ci-nightly.sh
set -euo pipefail
export LI_NIGHTLY=1
export LI_AST_FULL_SWEEP=1
export LI_PARSER_FULL_SWEEP=1
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "$ROOT/scripts/ci.sh"
