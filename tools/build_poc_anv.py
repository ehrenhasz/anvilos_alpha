#!/usr/bin/env python3
import os
import glob
import subprocess
import sys

# POC Builder
# Iterates over .mpy files in oss_sovereignty_mpy_poc and builds them to .anv

POC_ROOT = "oss_sovereignty_mpy_poc"
ANVIL_TOOL = "tools/anvil.py"

def main():
    print(">> [POC] Starting Batch Build of .mpy -> .anv")
    
    mpy_files = glob.glob(os.path.join(POC_ROOT, "**", "*.mpy"), recursive=True)
    
    success_count = 0
    fail_count = 0
    
    for mpy in mpy_files:
        # Determine output path (same dir, swap extension)
        anv = mpy.replace(".mpy", ".anv")
        
        print(f"Building: {mpy}")
        cmd = ["python3", ANVIL_TOOL, "build", mpy, "-o", anv]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"  [OK] {result.stdout.strip()}")
            success_count += 1
        else:
            print(f"  [FAIL] {result.stderr.strip()}")
            fail_count += 1

    print(f"\n>> [POC] Build Complete. Success: {success_count}, Fail: {fail_count}")
    if fail_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
