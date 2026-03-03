#!/usr/bin/env bash
set -euo pipefail

skip_gui=0
expected_tag=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-gui)
      skip_gui=1
      shift
      ;;
    --expected-tag)
      if [[ $# -lt 2 ]]; then
        echo "Error: --expected-tag requires a value" >&2
        exit 2
      fi
      expected_tag="$2"
      shift 2
      ;;
    *)
      echo "Usage: $0 [--skip-gui] [--expected-tag vX.Y.Z]" >&2
      exit 2
      ;;
  esac
done

echo "[release-check] Running test suite..."
make test

if [[ "$skip_gui" -eq 0 ]]; then
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists raylib; then
    echo "[release-check] Building GUI..."
    make gui
  else
    echo "[release-check] Skipping GUI build (raylib not detected)."
  fi
fi

if [[ -n "$expected_tag" ]]; then
  if ! git rev-parse -q --verify "refs/tags/$expected_tag" >/dev/null; then
    echo "[release-check] Missing tag: $expected_tag" >&2
    exit 1
  fi

  tag_commit="$(git rev-list -n 1 "$expected_tag")"
  head_commit="$(git rev-parse HEAD)"
  if [[ "$tag_commit" != "$head_commit" ]]; then
    echo "[release-check] Tag $expected_tag does not point to HEAD." >&2
    echo "  tag:  $tag_commit" >&2
    echo "  head: $head_commit" >&2
    exit 1
  fi
  echo "[release-check] Tag verification passed: $expected_tag -> HEAD"
fi

echo "[release-check] OK"
