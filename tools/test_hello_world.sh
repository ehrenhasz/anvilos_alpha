#!/bin/bash
set -e
echo ">>> ANVIL SOVEREIGNTY: HELLO WORLD TEST <<<"

# Setup
mkdir -p hello_world_test

# --- 1. Anvil Scripting (.mpy) ---
echo -e "\n[1] Testing .mpy (MicroPython Bytecode)..."
cat > hello_world_test/hello.py <<EOF
import sys
print("Hello from .mpy (Sovereign Runtime)!")
EOF

# Compile
./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross -march=x64 hello_world_test/hello.py

# Run (via import)
# We append the directory explicitly in python to ensure it's found
echo -e "import sys\nsys.path.append('hello_world_test')\nimport hello" | ./oss_sovereignty/sys_09_Anvil/build/anvil

# --- 2. Anvil Systems (.anv) ---
echo -e "\n[2] Testing .anv (Systems Language)..."
cat > hello_world_test/hello.anv <<EOF
fn main() -> i32 {
    let msg: &str = "Hello from .anv (Systems Language)!\n";
    printf(msg);
    0
}
EOF

# Transpile Headers
python3 oss_sovereignty/sys_09_Anvil/source/anvil.py header hello_world_test/kernel.h hello_world_test/hello.anv
# Patch Header (Add stdio.h for printf)
echo "#include <stdio.h>" | cat - hello_world_test/kernel.h > hello_world_test/kernel.h.tmp && mv hello_world_test/kernel.h.tmp hello_world_test/kernel.h

# Transpile Source
python3 oss_sovereignty/sys_09_Anvil/source/anvil.py transpile hello_world_test/hello.anv hello_world_test/hello.c

# Compile
CC="ext/toolchain/bin/x86_64-unknown-linux-musl-gcc"
if [ ! -f "$CC" ]; then 
    echo "Warning: Sovereign toolchain not found. Using host gcc."
    CC="gcc"
fi
$CC -o hello_world_test/hello_anv hello_world_test/hello.c -I hello_world_test -static

# Run
./hello_world_test/hello_anv

echo ">>> TEST COMPLETE <<<"
