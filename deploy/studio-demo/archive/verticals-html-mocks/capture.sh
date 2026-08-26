#!/usr/bin/env bash
# Capture PNG screenshots from vertical HTML mocks (1920×1080).
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-$DIR/png}"
CHROME="${CHROME:-}"
for c in "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" google-chrome chromium chromium-browser; do
  if [[ -x "$c" ]] || command -v "$c" >/dev/null 2>&1; then CHROME="$c"; break; fi
done
[[ -n "$CHROME" ]] || { echo "verticals/capture.sh: no chrome"; exit 0; }
mkdir -p "$OUT"

for f in "$DIR"/*.html; do
  [[ -f "$f" ]] || continue
  base=$(basename "$f" .html)
  timeout "${CAPTURE_CHROME_TIMEOUT_SEC:-30}" \
    "$CHROME" --headless --disable-gpu --hide-scrollbars \
    --window-size=1920,1080 \
    --screenshot="$OUT/${base}.png" \
    "file://${f}" || {
    echo "verticals/capture.sh: chrome timeout/fail for ${base}" >&2
    continue
  }
  echo "  $OUT/${base}.png"
done
