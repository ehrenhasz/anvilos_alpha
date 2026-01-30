#!/bin/bash
set -e

# Ensure we are in the script's directory
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR"

# Configuration (Relative to src/anvilos/kernel)
TOOLCHAIN_BIN="../../../ext/toolchain/bin"
CC="$TOOLCHAIN_BIN/x86_64-unknown-linux-musl-gcc"
CFLAGS="-m32 -static -nostdlib -fno-builtin -I. -Igenerated -I../../../oss_sovereignty/sys_09_Anvil/source"
OUT_DIR="../../../build_artifacts/phase4/bin"

# Ensure output dir exists
mkdir -p $OUT_DIR

# Clean previous build
rm -rf generated build-embed

# 1. Generate Embed Source (Ensure it's fresh)
echo "[BUILD] Generating MicroPython Embed Source..."
# We are already in SRC_DIR
make -f Makefile micropython-embed-package CC="$CC" CPP="$CC -E" > /dev/null

# 2. Compile Everything
echo "[BUILD] Compiling AnvilOS Kernel..."
# Collect sources
# Include bridge.c, generated sources, pyexec.c (Readline removed)
SRCS="bridge.c $(find generated -name "*.c") ../../../oss_sovereignty/sys_09_Anvil/source/shared/runtime/pyexec.c"

# Substrate path
SUBSTRATE="../../../build_artifacts/phase4/substrate.S"

# Compile and Link
# Note: We use -Wl,-Ttext=0x100000 to place kernel above 1MB
$CC $CFLAGS -Wl,-Ttext=0x100000 $SUBSTRATE $SRCS -o $OUT_DIR/anvil_unikernel

echo "[BUILD] Success: $OUT_DIR/anvil_unikernel"