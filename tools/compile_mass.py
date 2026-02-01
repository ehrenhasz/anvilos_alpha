#!/usr/bin/env python3
import os
import sys
import json
import subprocess
import glob

# AnvilOS Mass Compilation Tool (Chunked)
# Processes .mpy files in oss_sovereignty to .anv
# LIMIT: 100 files per run to ensure system stability.

REPO_ROOT = "oss_sovereignty"
STATE_FILE = "tools/compile_state.json"
FAILURE_LOG = "compile_failures.log"
ANVIL_TOOL = "tools/anvil.py"
CHUNK_SIZE = 100

def load_state():
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE, 'r') as f:
            return json.load(f)
    return {"processed_count": 0, "last_file": "", "success_count": 0, "fail_count": 0}

def save_state(processed_count, last_file, success_count, fail_count):
    with open(STATE_FILE, 'w') as f:
        json.dump({
            "processed_count": processed_count, 
            "last_file": last_file,
            "success_count": success_count,
            "fail_count": fail_count
        }, f)

def get_all_mpy_files():
    # Recursive walk, sorted for deterministic order
    mpy_files = []
    for root, dirs, files in os.walk(REPO_ROOT):
        for f in files:
            if f.endswith(".mpy"):
                full_path = os.path.join(root, f)
                mpy_files.append(full_path)
    
    return sorted(mpy_files)

def compile_file(mpy_path):
    anv_path = mpy_path.replace(".mpy", ".anv")
    cmd = [sys.executable, ANVIL_TOOL, "build", mpy_path, "-o", anv_path]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            return True, result.stdout.strip()
        else:
            # anvil.py prints the error to its stdout
            error_msg = result.stdout.strip() + "\n" + result.stderr.strip()
            return False, error_msg.strip()
    except Exception as e:
        return False, str(e)

def main():
    print(f">> [COMPILE] Starting Chunk (Limit: {CHUNK_SIZE})")
    state = load_state()
    all_files = get_all_mpy_files()
    
    start_index = 0
    if state["last_file"]:
        try:
            start_index = all_files.index(state["last_file"]) + 1
        except ValueError:
            start_index = 0 # Reset if file gone
            
    current_batch = all_files[start_index : start_index + CHUNK_SIZE]
    
    if not current_batch:
        print(">> [COMPILE] All files processed.")
        sys.exit(0)
        
    processed_in_chunk = 0
    success_in_chunk = 0
    fail_in_chunk = 0
    last_processed = state["last_file"]
    
    # Ensure failure log exists or is cleared if we are at the start? 
    # Better to append to failure log.
    
    for mpy in current_batch:
        success, msg = compile_file(mpy)
        if success:
            print(f"  [OK] {mpy}")
            success_in_chunk += 1
        else:
            print(f"  [FAIL] {mpy}")
            with open(FAILURE_LOG, 'a') as f:
                f.write(f"---" + " FAIL: " + mpy + " ---\\n"+msg+"\\n\\n")
            fail_in_chunk += 1
            
        last_processed = mpy
        processed_in_chunk += 1
        
    total_processed = state["processed_count"] + processed_in_chunk
    total_success = state["success_count"] + success_in_chunk
    total_fail = state["fail_count"] + fail_in_chunk
    
    save_state(total_processed, last_processed, total_success, total_fail)
    
    print(f">> [COMPILE] Chunk Complete. Total Success: {total_success}, Total Fail: {total_fail}")
    print(f">> [COMPILE] Progress: {start_index + processed_in_chunk} / {len(all_files)}")

if __name__ == "__main__":
    main()
