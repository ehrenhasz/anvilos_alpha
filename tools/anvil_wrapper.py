#!/usr/bin/env python3
import sys
import json
import subprocess
import os

MPY_CROSS = "./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross"

def main():
    if len(sys.argv) < 2:
        print("Usage: anvil <command> [args]")
        sys.exit(1)
    
    cmd = sys.argv[1]
    
    if cmd == "build":
        # Syntax: anvil build <source.mpy> -o <output.anv>
        if len(sys.argv) != 5 or sys.argv[3] != "-o":
            print("Usage: anvil build <source.mpy> -o <output.anv>")
            sys.exit(1)
            
        mpy_path = sys.argv[2]
        anv_path = sys.argv[4]
        
        with open(mpy_path, 'r') as f:
            data = json.load(f)
            
        source = data['human_readable_source']
        temp_py = mpy_path + ".temp.py"
        
        with open(temp_py, 'w') as f:
            f.write(source)
            
        try:
            result = subprocess.run([MPY_CROSS, "-o", anv_path, temp_py], capture_output=True, text=True)
            if result.returncode != 0:
                print(f"Compilation failed:\n{result.stderr}")
                sys.exit(1)
            print(f"Compiled {mpy_path} -> {anv_path}")
        finally:
            if os.path.exists(temp_py):
                os.remove(temp_py)
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)

if __name__ == "__main__":
    main()

