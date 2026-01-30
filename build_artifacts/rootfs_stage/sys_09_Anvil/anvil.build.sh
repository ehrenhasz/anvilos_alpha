#!/bin/bash
set -e

# ANVIL BUILD SCRIPT
# Target: x86_64-unknown-linux-musl
# Goal: Produce a hermetic, static 'anvil' binary.

# Get the directory of the script, then go up two levels to the project root
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
SOURCE_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/source"
TOOLCHAIN_BIN="$PROJECT_ROOT/ext/toolchain/bin/x86_64-unknown-linux-musl-"
OUTPUT_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/build"

mkdir -p "$OUTPUT_DIR"

echo ">> [BUILD] Phase 0: Building mpy-cross (Host Toolchain)..."
cd "$SOURCE_DIR/mpy-cross"
make clean
make -j$(nproc)

echo ">> [BUILD] Configuring MicroPython for Static Musl Build..."

cd "$SOURCE_DIR/ports/unix"

# Clean previous
make clean

# Build with static flags
# - Frozen manifest includes our anvil.py
# - Disabling features to lobotomize (no network, no dynamic loading)

export CC="${TOOLCHAIN_BIN}gcc"
export CXX="${TOOLCHAIN_BIN}g++"
export AR="${TOOLCHAIN_BIN}ar"
export LD="${TOOLCHAIN_BIN}ld"

# We use the standard MicroPython 'manifest.py' approach to freeze anvil.py
echo "freeze('$SOURCE_DIR', 'anvil.py')" > "$SOURCE_DIR/manifest.py"

echo ">> [BUILD] Running Make (Static)..."
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
    CFLAGS_EXTRA="-static" \
    LDFLAGS_EXTRA="-static" \
    FROZEN_MANIFEST="$SOURCE_DIR/manifest.py"

echo ">> [BUILD] Build Complete."
cp build-standard/micropython "$OUTPUT_DIR/anvil"
echo ">> [BUILD] Artifact: $OUTPUT_DIR/anvil"

# Strip the binary
"${TOOLCHAIN_BIN}strip" "$OUTPUT_DIR/anvil"
echo ">> [BUILD] Binary stripped."
