#!/usr/bin/env bash
# Native SDL viewport capture for Studio UI/UX plan loop (Xvfb when headless).
# Writes PNGs to STUDIO_UI_UX_NATIVE_PNG_DIR; metadata JSON on stdout.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE="$ROOT/deploy/studio-demo/native"
PNG_DIR="${STUDIO_UI_UX_NATIVE_PNG_DIR:-}"
PPM_DIR="${STUDIO_UI_UX_NATIVE_PPM_DIR:-$NATIVE/out}"
META="${STUDIO_UI_UX_NATIVE_META:-$ROOT/data/studio-ui-ux-plan-loop/latest-native-capture.json}"

if [[ -z "$PNG_DIR" ]]; then
  echo "capture-native: STUDIO_UI_UX_NATIVE_PNG_DIR required" >&2
  exit 2
fi

mkdir -p "$PNG_DIR" "$PPM_DIR"

if [[ "${STUDIO_UI_UX_CAPTURE_SKIP_NATIVE:-0}" == "1" ]]; then
  echo "capture-native: skipped (STUDIO_UI_UX_CAPTURE_SKIP_NATIVE=1)"
  python3 - "$META" <<'PY'
import json, sys
from pathlib import Path
Path(sys.argv[1]).write_text(json.dumps({
    "status": "skip",
    "skip_reason": "STUDIO_UI_UX_CAPTURE_SKIP_NATIVE=1",
    "native_pixels": False,
}, indent=2) + "\n", encoding="utf-8")
PY
  exit 0
fi

CAPTURE_SH="$NATIVE/capture.sh"
if [[ ! -f "$CAPTURE_SH" ]]; then
  echo "capture-native: missing $CAPTURE_SH" >&2
  exit 3
fi
chmod +x "$CAPTURE_SH" 2>/dev/null || true

STUDIO_VIEWPORT_CAPTURE_OUT="$PPM_DIR" \
  STUDIO_VIEWPORT_CAPTURE_FRAMES="${STUDIO_VIEWPORT_CAPTURE_FRAMES:-3}" \
  bash "$CAPTURE_SH" || {
    python3 - "$META" <<'PY'
import json, sys
from pathlib import Path
Path(sys.argv[1]).write_text(json.dumps({
    "status": "fail",
    "skip_reason": "SDL capture failed (missing sdl2/xvfb?)",
    "native_pixels": False,
}, indent=2) + "\n", encoding="utf-8")
PY
    exit 4
  }

python3 "$ROOT/scripts/studio-ppm-to-png.py" "$PPM_DIR" "$PNG_DIR" || exit 5

python3 - "$META" "$PNG_DIR" "$PPM_DIR" <<'PY'
import json, sys
from pathlib import Path

meta, png_dir, ppm_dir = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
pngs = sorted(png_dir.glob("*.png"))
native = len(pngs) > 0
meta.write_text(json.dumps({
    "status": "pass" if native else "fail",
    "native_pixels": native,
    "png_count": len(pngs),
    "png_dir": str(png_dir),
    "ppm_dir": str(ppm_dir),
    "capture_mode": "xvfb_sdl",
    "note": "SDL viewport stub — not full li-studio binary",
}, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"native_pixels": native, "png_count": len(pngs)}))
PY
