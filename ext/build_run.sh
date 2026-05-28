#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD="$ROOT/build"
EXE="$BUILD/Voxism"

cd "$SCRIPT_DIR"

if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
  echo "Configuring CMake (build/)..."
  cmake -S "$ROOT" -B "$BUILD" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
fi

echo "Building Voxism..."
cmake --build "$BUILD" --parallel

if [[ ! -x "$EXE" && ! -f "$EXE" ]]; then
  echo "Build finished but executable not found: $EXE" >&2
  exit 1
fi

echo "Running Voxism (cwd: build/)..."
(
  cd "$BUILD"
  exec "$EXE" "$@"
)
