#!/usr/bin/env bash
# Capture Studio UI evidence (PNG + optional MP4) and publish to GitHub — not in git.
#
# Env:
#   STUDIO_UI_UX_CAPTURE_DRY=1     — skip gh upload
#   STUDIO_UI_UX_TRACKING_ISSUE=N  — issue number for progress comments
#   GH_TOKEN / GITHUB_TOKEN        — required for upload
#   STUDIO_UI_UX_ITERATION=id      — label for artifact folder
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -n "${LI_CURSOR_AGENTS_ROOT:-}" ]]; then
  AGENTS_ROOT="$LI_CURSOR_AGENTS_ROOT"
elif [[ -d "$ROOT/../li-cursor-agents/ux-harness" ]]; then
  AGENTS_ROOT="$(cd "$ROOT/../li-cursor-agents" && pwd)"
elif [[ -d "$ROOT/../../li-cursor-agents/ux-harness" ]]; then
  AGENTS_ROOT="$(cd "$ROOT/../../li-cursor-agents" && pwd)"
else
  AGENTS_ROOT="$ROOT/../li-cursor-agents"
fi
STATE_DIR="$ROOT/data/studio-ui-ux-plan-loop"
ITER="${STUDIO_UI_UX_ITERATION:-$(date -u +%Y%m%dT%H%M%SZ)}"
ART="$STATE_DIR/artifacts/iter-$ITER"
PNG="$ART/png"
mkdir -p "$PNG"

REPO="${STUDIO_UI_UX_GH_REPO:-li-langverse/lic}"
RELEASE_TAG="${STUDIO_UI_UX_RELEASE_TAG:-studio-ui-ux-progress}"
ISSUE_FILE="$STATE_DIR/tracking-issue.txt"

capture_native_sdl() {
  if [[ "${STUDIO_UI_UX_CAPTURE_SKIP_NATIVE:-0}" == "1" ]]; then
    echo "capture: STUDIO_UI_UX_CAPTURE_SKIP_NATIVE=1 — skip native SDL"
    return 0
  fi
  local native_sh="$ROOT/scripts/studio-ui-ux-capture-native.sh"
  if [[ ! -f "$native_sh" ]]; then
    echo "capture: no studio-ui-ux-capture-native.sh"
    return 0
  fi
  chmod +x "$native_sh" 2>/dev/null || true
  local native_png="$PNG/native"
  mkdir -p "$native_png"
  export LIC_ROOT="$ROOT"
  if STUDIO_UI_UX_NATIVE_PNG_DIR="$native_png" bash "$native_sh"; then
    cp -a "$native_png/"*.png "$PNG/" 2>/dev/null || true
    echo "capture: native SDL viewport → $native_png"
  else
    echo "capture: native SDL skipped or failed (documented gap — HTML mocks remain)"
  fi
}

capture_html() {
  if [[ "${STUDIO_UI_UX_CAPTURE_SKIP_HTML:-0}" == "1" ]]; then
    echo "capture: STUDIO_UI_UX_CAPTURE_SKIP_HTML=1 — skip PNG"
    return 0
  fi
  local demo="$ROOT/deploy/studio-demo/screenshots"
  if [[ ! -d "$demo" ]]; then
    echo "capture: no deploy/studio-demo/screenshots — skip HTML"
    return 0
  fi
  local chrome=""
  if [[ -n "${CHROME:-}" ]]; then
    chrome="$CHROME"
  else
    for c in google-chrome chromium chromium-browser; do
      if command -v "$c" >/dev/null 2>&1; then chrome="$c"; break; fi
    done
  fi
  if [[ -z "$chrome" ]]; then
    echo "capture: no headless chrome — skip PNG"
    return 0
  fi
  export CHROME="$chrome"
  if [[ -x "$demo/capture.sh" ]]; then
    local html_timeout="${STUDIO_UI_UX_CAPTURE_HTML_TIMEOUT_SEC:-45}"
    if timeout "$html_timeout" env OUT="$demo/png" bash "$demo/capture.sh"; then
      cp -a "$demo/png/"*.png "$PNG/" 2>/dev/null || true
    else
      echo "capture: HTML screenshot timed out or failed (${html_timeout}s) — gap documented"
    fi
  fi
}

run_ux_harness() {
  if [[ ! -f "$AGENTS_ROOT/ux-harness/run_audit.py" ]]; then
    echo "capture: ux-harness missing"
    return 0
  fi
  export LIC_ROOT="$ROOT"
  python3 "$AGENTS_ROOT/ux-harness/run_audit.py" \
    --target world-studio-demo \
    --mode both \
    --mock \
    --out-dir "$ART" || true
  local native_out="$ART/native-audit"
  mkdir -p "$native_out"
  if [[ "${STUDIO_UI_UX_CAPTURE_SKIP_NATIVE:-0}" != "1" ]]; then
    python3 "$AGENTS_ROOT/ux-harness/run_audit.py" \
      --target world-studio-native \
      --mode both \
      --out-dir "$native_out" || true
    [[ -f "$native_out/ui-audit.json" ]] && cp "$native_out/ui-audit.json" "$STATE_DIR/latest-ui-audit-native.json"
    [[ -f "$native_out/ux-audit.json" ]] && cp "$native_out/ux-audit.json" "$STATE_DIR/latest-ux-audit-native.json"
  fi
  [[ -f "$ART/ui-audit.json" ]] && cp "$ART/ui-audit.json" "$STATE_DIR/latest-ui-audit.json"
  [[ -f "$ART/ux-audit.json" ]] && cp "$ART/ux-audit.json" "$STATE_DIR/latest-ux-audit.json"
}

make_reel() {
  if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "capture: ffmpeg missing — skip video"
    return 0
  fi
  shopt -s nullglob
  local files=("$PNG"/*.png)
  if [[ ${#files[@]} -eq 0 ]]; then
    return 0
  fi
  local list="$ART/ffmpeg-list.txt"
  : >"$list"
  for f in "${files[@]}"; do
    printf "file '%s'\nduration 2\n" "$f" >>"$list"
  done
  printf "file '%s'\n" "${files[-1]}" >>"$list"
  ffmpeg -y -f concat -safe 0 -i "$list" -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2" \
    -c:v libx264 -pix_fmt yuv420p "$ART/iter-reel.mp4" 2>/dev/null || echo "capture: ffmpeg failed"
}

write_report() {
  local report="$ART/report.md"
  {
    echo "## Studio UI/UX iteration \`$ITER\`"
    echo ""
    echo "- **UTC:** $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "- **Branch:** $(git -C "$ROOT" branch --show-current 2>/dev/null || echo unknown)"
    if [[ -f "$ROOT/data/studio-ui-ux-plan-loop/latest-bench.json" ]]; then
      echo ""
      echo "### Bench snapshot"
      echo '```json'
      head -c 4000 "$ROOT/data/studio-ui-ux-plan-loop/latest-bench.json"
      echo '```'
    fi
    echo ""
    echo "### Artifacts"
    echo "- PNG count: $(ls -1 "$PNG"/*.png 2>/dev/null | wc -l)"
    [[ -f "$ART/iter-reel.mp4" ]] && echo "- Video: \`iter-reel.mp4\` (GitHub release only)"
    echo ""
    echo "Rubric: \`docs/game-dev/competitive-intel/ui-ux-by-dimension.md\`"
  } >"$report"
  echo "$report"
}

write_capture_meta() {
  local issue="${1:-}" comment_url="${2:-}" upload_count="${3:-0}" dry="${4:-0}"
  local meta="$STATE_DIR/latest-capture.json"
  local png_count
  png_count="$(ls -1 "$PNG"/*.png 2>/dev/null | wc -l | tr -d ' ')"
  python3 - "$meta" "$ITER" "$REPO" "$RELEASE_TAG" "$issue" "$comment_url" \
    "$png_count" "$upload_count" "$dry" "$ART" "$ROOT" <<'PY'
import json, sys
from pathlib import Path
meta, iteration, repo, tag, issue, comment_url = sys.argv[1:7]
png_count, upload_count, dry, art, root = int(sys.argv[7]), int(sys.argv[8]), sys.argv[9], sys.argv[10], Path(sys.argv[11])
native_meta = root / "data/studio-ui-ux-plan-loop/latest-native-capture.json"
native_pixels = False
capture_mode = "html_mock"
if native_meta.is_file():
    try:
        nm = json.loads(native_meta.read_text(encoding="utf-8"))
        native_pixels = bool(nm.get("native_pixels"))
        capture_mode = nm.get("capture_mode", capture_mode)
    except json.JSONDecodeError:
        pass
Path(meta).write_text(json.dumps({
    "iteration": iteration,
    "artifact_dir": art,
    "repo": repo,
    "release_tag": tag,
    "tracking_issue": int(issue) if issue.isdigit() else None,
    "issue_url": f"https://github.com/{repo}/issues/{issue}" if issue.isdigit() else None,
    "issue_comment_url": comment_url or None,
    "release_url": f"https://github.com/{repo}/releases/tag/{tag}",
    "png_count": png_count,
    "upload_count": upload_count,
    "has_reel": (Path(art) / "iter-reel.mp4").is_file(),
    "dry_run": dry == "1",
    "native_pixels": native_pixels,
    "capture_mode": capture_mode,
}, indent=2) + "\n", encoding="utf-8")
PY
}

publish_github() {
  local dry="${STUDIO_UI_UX_CAPTURE_DRY:-0}"
  local issue="" comment_url="" upload_count=0

  if [[ "$dry" == "1" ]]; then
    echo "capture: dry run — skip GitHub publish"
    write_report >/dev/null
    write_capture_meta "" "" 0 1
    return 0
  fi
  if ! command -v gh >/dev/null 2>&1; then
    echo "capture: gh CLI missing"
    write_capture_meta "" "" 0 0
    return 0
  fi
  local token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
  if [[ -z "$token" ]]; then
    echo "capture: no GH_TOKEN — skip publish"
    write_capture_meta "" "" 0 0
    return 0
  fi

  issue="${STUDIO_UI_UX_TRACKING_ISSUE:-}"
  if [[ -z "$issue" && -f "$ISSUE_FILE" ]]; then
    issue="$(tr -d '[:space:]' <"$ISSUE_FILE")"
  fi
  if [[ -z "$issue" ]]; then
    echo "capture: creating tracking issue"
    local created
    created="$(gh issue create --repo "$REPO" \
      --title "Studio UI/UX plan loop — progress tracker" \
      --body "Automated progress from \`studio-ui-ux-capture-progress.sh\`. Do not attach large binaries to PRs — assets go to release \`$RELEASE_TAG\`." \
      --label "studio-ui-ux" 2>/dev/null || \
      gh issue create --repo "$REPO" \
        --title "Studio UI/UX plan loop — progress tracker" \
        --body "Automated progress from \`studio-ui-ux-capture-progress.sh\`. Do not attach large binaries to PRs — assets go to release \`$RELEASE_TAG\`." 2>/dev/null || true)"
    issue="$(echo "$created" | grep -oE '[0-9]+$' || true)"
    [[ -n "$issue" ]] && echo "$issue" >"$ISSUE_FILE"
  fi

  local report
  report="$(write_report)"
  if [[ -n "$issue" ]]; then
    comment_url="$(gh issue comment "$issue" --repo "$REPO" --body-file "$report" 2>/dev/null || true)"
    if [[ -n "$comment_url" ]]; then
      echo "capture: issue comment → $comment_url"
    else
      echo "capture: commented on issue #$issue (https://github.com/$REPO/issues/$issue)"
    fi
  fi

  if gh release view "$RELEASE_TAG" --repo "$REPO" >/dev/null 2>&1; then
    :
  else
    gh release create "$RELEASE_TAG" --repo "$REPO" \
      --title "Studio UI/UX progress (rolling)" \
      --notes "Rolling assets from plan loop. Not versioned product releases." || true
  fi
  shopt -s nullglob
  local uploads=("$PNG"/*.png)
  [[ -f "$ART/iter-reel.mp4" ]] && uploads+=("$ART/iter-reel.mp4")
  if [[ ${#uploads[@]} -gt 0 ]]; then
    if gh release upload "$RELEASE_TAG" "${uploads[@]}" --repo "$REPO" --clobber 2>/dev/null; then
      upload_count=${#uploads[@]}
      echo "capture: uploaded ${upload_count} file(s) to https://github.com/$REPO/releases/tag/$RELEASE_TAG"
    else
      echo "capture: release upload failed (see gh auth / repo permissions)"
    fi
  fi
  write_capture_meta "$issue" "$comment_url" "$upload_count" 0
}

echo "==> studio-ui-ux capture ($ITER)"
capture_native_sdl
capture_html
run_ux_harness
"$ROOT/scripts/bench-studio-viewport-perf.sh" || true
make_reel
write_report >/dev/null
publish_github
echo "capture: done → $ART"
