#!/usr/bin/env bash
# Record Li World Studio per-vertical demo MP4 from native SDL frames only (no HTML mocks).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MEDIA="$ROOT/docs/demo/media"
PNG="${STUDIO_VERTICALS_NATIVE_PNG_DIR:-$MEDIA/native-verticals/png}"
DRY_RUN="${STUDIO_VERTICALS_DRY_RUN:-0}"
VERT_IDS=(game sim_rl sim_automotive sim_robotics sim_additive sim_scientific sim_drug_design)

mkdir -p "$MEDIA"

if [[ "$DRY_RUN" == "1" ]]; then
  echo "record-studio-verticals-demo: dry-run — native capture then encode"
  echo "  capture -> $PNG/{${VERT_IDS[*]}}.png"
  echo "  encode  -> $MEDIA/studio-verticals-demo.mp4"
  exit 0
fi

export LIG_HOST_PRESENT=1
CAPTURE="$ROOT/scripts/studio-verticals-capture-native.sh"
chmod +x "$CAPTURE" 2>/dev/null || true
if ! bash "$CAPTURE"; then
  echo "record-studio-verticals-demo: NO_MP4_NATIVE — native capture failed" >&2
  echo "  See docs/demo/VERTICALS-RECORDING.md (blocker + repro)" >&2
  echo "  Fix: install SDL2; run LIG_HOST_PRESENT=1 $CAPTURE" >&2
  exit 1
fi

if [[ ! -d "$PNG" ]] || [[ -z "$(find "$PNG" -maxdepth 1 -name '*.png' -print -quit 2>/dev/null)" ]]; then
  echo "record-studio-verticals-demo: NO_MP4_NATIVE — no native frame dir: $PNG" >&2
  exit 1
fi

for id in "${VERT_IDS[@]}"; do
  if [[ ! -f "$PNG/${id}.png" ]]; then
    echo "record-studio-verticals-demo: missing native frame $PNG/${id}.png" >&2
    exit 1
  fi
done

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "record-studio-verticals-demo: install ffmpeg" >&2
  exit 3
fi

LIST="$MEDIA/ffmpeg-verticals-scenes.txt"
: >"$LIST"
dur="${STUDIO_VERTICAL_SCENE_SEC:-10}"
for id in "${VERT_IDS[@]}"; do
  png="$PNG/${id}.png"
  echo "file '${png}'" >>"$LIST"
  echo "duration ${dur}" >>"$LIST"
done
echo "file '${PNG}/game.png'" >>"$LIST"
echo "duration 5" >>"$LIST"

echo "record-studio-verticals-demo: encoding MP4 (native frames only)"
ffmpeg -y -f concat -safe 0 -i "$LIST" \
  -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2" \
  -c:v libx264 -pix_fmt yuv420p -r 30 \
  "$MEDIA/studio-verticals-demo.mp4" 2>/dev/null

ffdur=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \
  "$MEDIA/studio-verticals-demo.mp4" 2>/dev/null || echo "?")
echo "record-studio-verticals-demo: done → $MEDIA/studio-verticals-demo.mp4 (${ffdur}s, native_pixels=1)"
echo "Honesty matrix → docs/demo/VERTICALS-RECORDING.md"
