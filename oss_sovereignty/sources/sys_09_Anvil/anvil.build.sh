set -e
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
SOURCE_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/source"
TOOLCHAIN_BIN="$PROJECT_ROOT/ext/toolchain/bin/x86_64-unknown-linux-musl-"
OUTPUT_DIR="$PROJECT_ROOT/oss_sovereignty/sys_09_Anvil/build"
mkdir -p "$OUTPUT_DIR"
echo ">> [BUILD] Phase 0: Building mpy-cross (Host Toolchain)..."
cd "$SOURCE_DIR/mpy-cross"
make clean
make -j$(nproc) CFLAGS_EXTRA="-static" LDFLAGS_EXTRA="-static"
echo ">> [BUILD] Configuring MicroPython for Static Musl Build..."
cd "$SOURCE_DIR/ports/unix"
make clean
export CC="${TOOLCHAIN_BIN}gcc"
export CXX="${TOOLCHAIN_BIN}g++"
export AR="${TOOLCHAIN_BIN}ar"
export LD="${TOOLCHAIN_BIN}ld"
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
"${TOOLCHAIN_BIN}strip" "$OUTPUT_DIR/anvil"
echo ">> [BUILD] Binary stripped."
