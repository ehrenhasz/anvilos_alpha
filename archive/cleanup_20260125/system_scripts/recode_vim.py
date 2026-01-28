import os
import json

TARGET_DIR = 'oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic'
BUILD_SCRIPT = os.path.join(TARGET_DIR, 'anvil.build.sh')
METADATA_FILE = os.path.join(TARGET_DIR, 'metadata.json')

def main():
    if not os.path.exists(TARGET_DIR):
        print(f'Error: {TARGET_DIR} missing')
        return

    # 1. Create anvil.build.sh
    build_content = """#!/bin/bash
set -e
# ANVIL BUILD: Vim (Static/Minimal)

PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../.." && pwd )"
SOURCE_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/source"
BUILD_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/build"
INSTALL_DIR="$PROJECT_ROOT/oss_sovereignty/sys_99_Legacy_Bin/EDITORS_DOCS/vim.basic/dist"

mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

echo ">> [ANVIL] Configuring Vim..."
cd "$SOURCE_DIR"

# Clean if needed
make distclean || true

# Configure for Static Musl
# We assume 'anvil' binary or toolchain is available in path or ext/toolchain
export CC="$PROJECT_ROOT/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc"
export CFLAGS="-static -Os"
export LDFLAGS="-static"

./configure \
    --prefix="$INSTALL_DIR" \
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
    --disable-sysmouse

echo ">> [ANVIL] Building Vim..."
make -j$(nproc)

echo ">> [ANVIL] Installing Vim..."
make install

echo ">> [ANVIL] Build Complete: $INSTALL_DIR/bin/vim"
"""

    with open(BUILD_SCRIPT, 'w') as f:
        f.write(build_content)
    
    os.chmod(BUILD_SCRIPT, 0o755)
    print(f'Created {BUILD_SCRIPT}')

    # 2. Update Metadata
    metadata = {
        'id': 'vim.basic',
        'name': 'Vim (Anvil Edition)',
        'version': '9.1',
        'type': 'sovereign_tool',
        'build_script': 'anvil.build.sh',
        'notes': 'Statically linked, features=small, no interpreters.'
    }
    
    with open(METADATA_FILE, 'w') as f:
        json.dump(metadata, f, indent=2)
        
    print(f'Updated {METADATA_FILE}')

if __name__ == '__main__':
    main()
