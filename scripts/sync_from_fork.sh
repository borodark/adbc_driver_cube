#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/adbc" >&2
  exit 1
fi

SRC_ROOT="$1"
SRC_DIR="$SRC_ROOT/3rd_party/apache-arrow-adbc"
DEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/third_party/apache-arrow-adbc"

if [[ ! -d "$SRC_DIR" ]]; then
  echo "ADBC source not found at $SRC_DIR" >&2
  exit 1
fi

rm -rf "$DEST_DIR"
mkdir -p "$(dirname "$DEST_DIR")"
cp -R "$SRC_DIR" "$DEST_DIR"

echo "Copied $SRC_DIR -> $DEST_DIR"
