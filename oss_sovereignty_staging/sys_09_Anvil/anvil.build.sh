#!/bin/bash
set -e

# ANVIL BUILD SCRIPT
# Target: x86_64-unknown-linux-musl (Sovereign Toolchain)
# Goal: Produce a hermetic, static 'anvil' binary with subprocess capabilities.

PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
SOURCE_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/source"
TOOLCHAIN_BIN="$PROJECT_ROOT/ext/toolchain/bin/x86_64-unknown-linux-musl-"
OUTPUT_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/build"

# Set Sovereign Toolchain for cross-compilation context
export CROSS_COMPILE="${TOOLCHAIN_BIN}"
# Ensure standard variables are set for Makefiles that might ignore CROSS_COMPILE
export CC="${TOOLCHAIN_BIN}gcc"
export CXX="${TOOLCHAIN_BIN}g++"
export AR="${TOOLCHAIN_BIN}ar"
export LD="${TOOLCHAIN_BIN}ld"
export STRIP="${TOOLCHAIN_BIN}strip"

mkdir -p "$OUTPUT_DIR"

echo ">> [BUILD] Phase 0: Building mpy-cross (Sovereign Static)..."
cd "$SOURCE_DIR/mpy-cross"
make clean
# Build static mpy-cross using sovereign toolchain.
# CFLAGS_EXTRA="-static" ensures it's a static binary executable on the host.
make -j$(nproc) CC="$CC" CFLAGS_EXTRA="-static" LDFLAGS_EXTRA="-static"

echo ">> [BUILD] Configuring MicroPython for Static Musl Build..."
cd "$SOURCE_DIR/ports/unix"
make clean

# Create manifest to freeze anvil.py
echo "freeze('$SOURCE_DIR', 'anvil.py')" > "$SOURCE_DIR/manifest.py"

echo ">> [BUILD] Running Make (Static)..."
# We invoke make. Since we exported CROSS_COMPILE and CC, it should be fine.
# We pass CFLAGS_EXTRA="-static" for the runtime.
make -j$(nproc) \
    MICROPY_PY_BTREE=0 \
    MICROPY_PY_TERMIOS=0 \
    MICROPY_PY_SOCKET=0 \
    MICROPY_PY_NETWORK=0 \
    MICROPY_PY_SSL=0 \
    MICROPY_PY_AXTLS=0 \
    MICROPY_PY_FFI=0 \
    MICROPY_PY_JNI=0 \
    MICROPY_PY_BLUETOOTH=0 \
    MPY_LIB_DIR=../.. \
    MICROPY_USE_READLINE=1 \
    CFLAGS_EXTRA="-static -Wno-error=unterminated-string-initialization" \
    LDFLAGS_EXTRA="-static" \
    FROZEN_MANIFEST="$SOURCE_DIR/manifest.py"

echo ">> [BUILD] Build Complete."
cp build-standard/micropython "$OUTPUT_DIR/anvil"
echo ">> [BUILD] Artifact: $OUTPUT_DIR/anvil"

"$STRIP" "$OUTPUT_DIR/anvil"
echo ">> [BUILD] Binary stripped."