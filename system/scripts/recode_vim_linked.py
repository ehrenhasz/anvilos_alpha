import os
import json

TARGET_DIR = 'oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic'
BUILD_SCRIPT = os.path.join(TARGET_DIR, 'anvil.build.sh')

def main():
    if not os.path.exists(TARGET_DIR):
        print(f'Error: {TARGET_DIR} missing')
        return

    # Update build script to link against Ncurses
    build_content = """#!/bin/bash
set -e
# ANVIL BUILD: Vim (Static/Minimal)

PROJECT_ROOT=\"$( cd \"$( dirname \"${BASH_SOURCE[0]}\" )/../../../..\" && pwd )\" # This line has been corrected to properly escape the inner quotes
SOURCE_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/source"
BUILD_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/build"
INSTALL_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/dist"

# Dependencies
NCURSES_DIR="$PROJECT_ROOT/oss_sovereignty/sys_03_Libraries/ncurses/dist"

mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

echo ">> [ANVIL] Configuring Vim..."
cd "$SOURCE_DIR"

# Clean if needed
make distclean || true

# Configure for Static Musl with Local Ncurses
export CC="$PROJECT_ROOT/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc"
# Add Ncurses include path
export CFLAGS="-static -Os -I$NCURSES_DIR/include -I$NCURSES_DIR/include/ncurses"
# Add Ncurses lib path and force link against tinfo/ncurses
export LDFLAGS="-static -L$NCURSES_DIR/lib"
# LIBS might be needed if configure doesn't pick up LDFLAGS for library searching correctly
export LIBS="-lncurses -ltinfo"

./configure \
    --prefix=\"$INSTALL_DIR\" \
    --with-features=small \
    --disable-gui \
    --without-x \
    --disable-netbeans \
    --disable-pythoninterp \
    --disable-perlinterp \
    --disable-rubyinterp \
    --disable-luainterp \
    --disable-tclinterp \
    --enable-multibyte \
    --disable-nls \
    --disable-selinux \
    --disable-gpm \
    --disable-sysmouse \
    --with-tlib=ncurses

echo ">> [ANVIL] Building Vim..."
make -j$(nproc)

echo ">> [ANVIL] Installing Vim..."
make install

echo ">> [ANVIL] Build Complete: $INSTALL_DIR/bin/vim"
"""

    with open(BUILD_SCRIPT, 'w') as f:
        f.write(build_content)
    
    os.chmod(BUILD_SCRIPT, 0o755)
    print(f'Updated {BUILD_SCRIPT}')

if __name__ == '__main__':
    main()
