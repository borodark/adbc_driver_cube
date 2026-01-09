#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/dist}"
TRIPLET="${TRIPLET:-x86_64-linux-gnu}"
VERSION="${VERSION:-}"

if [[ -z "$VERSION" ]]; then
  VERSION=$(git -C "$ROOT_DIR" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
fi
if [[ -z "$VERSION" ]]; then
  VERSION=0.0.0
fi

LIB_PATH=$(find "$BUILD_DIR" -name "libadbc_driver_cube.so" -print -quit)
if [[ -z "$LIB_PATH" ]]; then
  echo "libadbc_driver_cube.so not found in $BUILD_DIR" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
STAGING="$OUT_DIR/stage"
rm -rf "$STAGING"
mkdir -p "$STAGING"
cp "$LIB_PATH" "$STAGING/"

TARBALL="$OUT_DIR/adbc_driver_cube-${VERSION}-${TRIPLET}.tar.gz"
tar -C "$STAGING" -czf "$TARBALL" .

echo "Wrote $TARBALL"
