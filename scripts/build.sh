#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADBC_SRC="${ADBC_SRC:-$ROOT_DIR/third_party/apache-arrow-adbc}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"

if [[ ! -d "$ADBC_SRC/c" ]]; then
  echo "ADBC sources not found at $ADBC_SRC/c" >&2
  echo "Run: ./scripts/sync_from_fork.sh /path/to/adbc" >&2
  exit 1
fi

cmake -S "$ADBC_SRC/c" -B "$BUILD_DIR" \
  -DADBC_DRIVER_CUBE=ON \
  -DADBC_BUILD_SHARED=ON \
  -DADBC_DRIVER_MANAGER=OFF

cmake --build "$BUILD_DIR" --target adbc_driver_cube_shared --config Release
