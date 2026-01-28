#!/bin/bash
set -e

TOOLCHAIN_URL="https://github.com/cross-tools/musl-cross/releases/download/20250929/x86_64-unknown-linux-musl.tar.xz"
DEST_DIR="ext/toolchain"
TMP_ARCHIVE="ext/toolchain.tar.xz"

echo ">> [TOOLCHAIN] Preparing destination: $DEST_DIR"
mkdir -p "$DEST_DIR"

echo ">> [TOOLCHAIN] Downloading x86_64-unknown-linux-musl..."
wget -q -O "$TMP_ARCHIVE" "$TOOLCHAIN_URL"

echo ">> [TOOLCHAIN] Extracting..."
tar -xf "$TMP_ARCHIVE" -C "$DEST_DIR" --strip-components=1

echo ">> [TOOLCHAIN] Cleanup..."
rm "$TMP_ARCHIVE"

echo ">> [TOOLCHAIN] Ready at $DEST_DIR"
