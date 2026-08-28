#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MP4="$ROOT/docs/demo/media/studio-verticals-demo.mp4"
TAG="${1:?usage: $0 <release-tag>}"
if [[ ! -f "$MP4" ]]; then echo "missing $MP4" >&2; exit 1; fi
command -v gh >/dev/null || { echo "install gh" >&2; exit 2; }
gh release upload "$TAG" "$MP4" --clobber
