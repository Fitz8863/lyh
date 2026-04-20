#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_CMAKE="$ROOT_DIR/CMakeLists.txt"
MAIN_LINK="$ROOT_DIR/build/CMakeFiles/main.dir/link.txt"

if grep -q 'camera_capture_helper' "$MAIN_CMAKE"; then
  echo "FAIL: camera_capture_helper target still exists"
  exit 1
fi

if [[ ! -f "$MAIN_LINK" ]]; then
  echo "FAIL: main link.txt not found; build the project first"
  exit 1
fi

if grep -q 'libturbojpeg' "$MAIN_LINK"; then
  echo "FAIL: main still links libturbojpeg"
  exit 1
fi

echo "PASS: single-process capture linkage is clean"
