#!/usr/bin/env bash
# Merge libFuzzer corpus + minimized crash artifacts into compiler/fuzz/corpus/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FUZZ_BIN="${1:?usage: merge_fuzz_corpus.sh FUZZ_BIN REPO_CORPUS WORK_CORPUS [ARTIFACT_DIR]}"
REPO_CORPUS="${2:?}"
WORK_CORPUS="${3:?}"
ARTIFACT_DIR="${4:-}"

MAX_FILES="${LI_FUZZ_MAX_CORPUS:-500}"
MAX_BYTES=$((1 << 20))

mkdir -p "$REPO_CORPUS/regressions"

# Merge coverage-guided inputs into repo corpus (dedupe via libFuzzer).
MERGE_OUT="$(mktemp -d)"
trap 'rm -rf "$MERGE_OUT"' EXIT

if [[ -x "$FUZZ_BIN" ]]; then
  "$FUZZ_BIN" -merge=1 "$MERGE_OUT" "$REPO_CORPUS" "$WORK_CORPUS" 2>/dev/null || true
  for f in "$MERGE_OUT"/*; do
    [[ -f "$f" ]] || continue
    base="$(basename "$f")"
    [[ -f "$REPO_CORPUS/$base" ]] && continue
    sz=$(wc -c <"$f" | tr -d ' ')
    if (( sz > MAX_BYTES )); then
      continue
    fi
    cp "$f" "$REPO_CORPUS/$base"
  done
fi

# Promote crash artifacts as regression seeds.
if [[ -n "$ARTIFACT_DIR" && -d "$ARTIFACT_DIR" ]]; then
  for crash in "$ARTIFACT_DIR"/crash-*; do
    [[ -f "$crash" ]] || continue
    sz=$(wc -c <"$crash" | tr -d ' ')
    if (( sz > MAX_BYTES )); then
      continue
    fi
  done
  for crash in "$ARTIFACT_DIR"/crash-*; do
    [[ -f "$crash" ]] || continue
    sz=$(wc -c <"$crash" | tr -d ' ')
    (( sz <= MAX_BYTES )) || continue
    if [[ -x "$FUZZ_BIN" ]]; then
      MIN="$(mktemp)"
      if "$FUZZ_BIN" "$MIN" -minimize_crash=1 "$crash" 2>/dev/null; then
        crash="$MIN"
      fi
    fi
    hash=$(shasum -a 256 "$crash" | cut -c1-12)
    dest="$REPO_CORPUS/regressions/regression_${hash}"
    cp "$crash" "$dest"
    [[ "${MIN:-}" != "$crash" ]] && rm -f "${MIN:-}"
  done
fi

# Cap corpus size (keep regressions, trim oldest flat seeds).
count=$(find "$REPO_CORPUS" -maxdepth 1 -type f | wc -l | tr -d ' ')
if (( count > MAX_FILES )); then
  find "$REPO_CORPUS" -maxdepth 1 -type f -printf '%T@ %p\n' | sort -n | head -n $((count - MAX_FILES)) |
    while read -r _ path; do
      rm -f "$path"
    done
fi

echo "merge_fuzz_corpus: $(find "$REPO_CORPUS" -type f | wc -l | tr -d ' ') files under $REPO_CORPUS"
