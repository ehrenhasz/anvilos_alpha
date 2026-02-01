#!/usr/bin/env python3
import subprocess
import os

ANVIL_TOOL = "tools/anvil.py"
BATCH_FILE = "tools/anv_batch_1.txt"

def main():
    if not os.path.exists(BATCH_FILE):
        print(f"Error: {BATCH_FILE} not found.")
        return

    with open(BATCH_FILE, 'r') as f:
        mpy_files = [line.strip() for line in f if line.strip()]

    print(f">> [ANVIL] Compiling {len(mpy_files)} files to .anv")
    
    success = 0
    fail = 0
    
    for mpy in mpy_files:
        anv = mpy.replace(".mpy", ".anv")
        cmd = ["python3", ANVIL_TOOL, "build", mpy, "-o", anv]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            success += 1
        else:
            print(f"  [FAIL] {mpy}: {result.stderr.strip()}")
            fail += 1

    print(f">> [ANVIL] Batch 1 Complete. Success: {success}, Fail: {fail}")

if __name__ == "__main__":
    main()
