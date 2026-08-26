#!/usr/bin/env bash
# Fail when AutoVC.lean has Prop obligations without a matching _proved theorem (2f gate).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
AUTOVC="${1:-$ROOT/build/generated/AutoVC.lean}"
if [[ ! -f "$AUTOVC" ]]; then
  echo "check-autovc-open-goals: missing $AUTOVC" >&2
  exit 1
fi
open=0
while IFS= read -r line; do
  if [[ "$line" =~ ^def\ vc_.*decreases.*:\ Nat\ := ]]; then
    name="${line#def }"
    name="${name%% *}"
    if ! grep -q "theorem ${name}_proved" "$AUTOVC"; then
      echo "open VC (decreases): $name" >&2
      open=$((open + 1))
    fi
    continue
  fi
  if [[ "$line" =~ ^def\ vc_.*requires.*:\ Prop\ := ]] && [[ ! "$line" =~ :=\ True$ ]]; then
    continue
  fi
  if [[ "$line" =~ ^def\ vc_.*\ :\ Prop\ :=\ True$ ]]; then
    name="${line#def }"
    name="${name%% *}"
    if ! grep -q "theorem ${name}_proved" "$AUTOVC"; then
      echo "open VC (true): $name" >&2
      open=$((open + 1))
    fi
    continue
  fi
  if [[ "$line" =~ ^def\ vc_.*\ :\ Prop\ := ]]; then
    name="${line#def }"
    name="${name%% *}"
    if ! grep -q "theorem ${name}_proved" "$AUTOVC"; then
      echo "open VC: $name" >&2
      open=$((open + 1))
    fi
  fi
done < "$AUTOVC"
if [[ "$open" -gt 0 ]]; then
  echo "check-autovc-open-goals: $open open obligation(s) in $AUTOVC" >&2
  exit 1
fi
echo "check-autovc-open-goals: ok (no open Prop goals)"
