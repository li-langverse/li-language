#!/usr/bin/env bash
# Create ecosystem-upstream label on li-langverse repos (idempotent).
set -euo pipefail
WRAPPER="$(cd "$(dirname "$0")/.." && pwd)/scripts/with-github-env.sh"
for repo in lic lip lit; do
  echo "==> li-langverse/${repo}"
  "$WRAPPER" gh label create ecosystem-upstream \
    --repo "li-langverse/${repo}" \
    --color "1D76DB" \
    --description "Upstream lic/lit/lip release — bump toolchain pins" \
    2>/dev/null || "$WRAPPER" gh label edit ecosystem-upstream \
      --repo "li-langverse/${repo}" \
      --description "Upstream lic/lit/lip release — bump toolchain pins"
done
echo "setup-ecosystem-labels: ok"
