#!/usr/bin/env python3
import os
import sys
import json
import hashlib
import glob

# AnvilOS Ingestion Tool (Chunked)
# Migrates files from oss_sovereignty_staging to oss_sovereignty (as .mpy)
# LIMIT: 100 files per run to ensure system stability.

STAGING_ROOT = "oss_sovereignty_staging"
REPO_ROOT = "oss_sovereignty"
STATE_FILE = "tools/ingest_state.json"
CHUNK_SIZE = 100

def load_state():
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE, 'r') as f:
            return json.load(f)
    return {"processed_count": 0, "last_file": ""}

def save_state(processed_count, last_file):
    with open(STATE_FILE, 'w') as f:
        json.dump({"processed_count": processed_count, "last_file": last_file}, f)

def get_all_files():
    # Recursive walk, sorted for deterministic order
    all_files = []
    for root, dirs, files in os.walk(STAGING_ROOT):
        # Ignore .git and build artifacts
        if ".git" in root: continue
        
        for f in files:
            if f.endswith(".o") or f.endswith(".a") or f.endswith(".lo"): continue
            
            full_path = os.path.join(root, f)
            all_files.append(full_path)
    
    return sorted(all_files)

def ingest_file(src_path):
    try:
        # Determine relative path for mirroring
        rel_path = os.path.relpath(src_path, STAGING_ROOT)
        dest_path = os.path.join(REPO_ROOT, rel_path + ".mpy")
        
        # Read content
        try:
            with open(src_path, 'r', encoding='utf-8') as f:
                content = f.read()
                is_binary = False
        except UnicodeDecodeError:
            # Simple binary handling for now (placeholder or fail)
            # For Phase 0.x, we focus on source. If binary, we might skip or base64.
            # Let's skip true binaries for now to focus on source code.
            return False, "Skipped Binary"

        # Calculate Hash
        file_hash = hashlib.sha256(content.encode('utf-8')).hexdigest()
        
        # Build MicroJSON
        data = {
            "module_name": os.path.basename(src_path),
            "hash_id": file_hash,
            "original_prompt": f"Ingested from {rel_path}",
            "human_readable_source": content,
            "logic_map": {},
            "failure_modes": [],
            "crash_correlation_map": {}
        }
        
        # Ensure directory exists
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        
        # Write .mpy
        with open(dest_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
            
        return True, dest_path
        
    except Exception as e:
        return False, str(e)

def main():
    print(f">> [INGEST] Starting Chunk (Limit: {CHUNK_SIZE})")
    state = load_state()
    all_files = get_all_files()
    
    start_index = 0
    if state["last_file"]:
        try:
            start_index = all_files.index(state["last_file"]) + 1
        except ValueError:
            start_index = 0 # Reset if file gone
            
    current_batch = all_files[start_index : start_index + CHUNK_SIZE]
    
    if not current_batch:
        print(">> [INGEST] All files processed.")
        sys.exit(0)
        
    processed_in_chunk = 0
    last_processed = state["last_file"]
    
    for src in current_batch:
        success, msg = ingest_file(src)
        if success:
            print(f"  [OK] {src} -> .mpy")
        else:
            print(f"  [SKIP] {src} ({msg})")
            
        last_processed = src
        processed_in_chunk += 1
        
    total_processed = state["processed_count"] + processed_in_chunk
    save_state(total_processed, last_processed)
    
    print(f">> [INGEST] Chunk Complete. Total Processed: {total_processed}")
    print(f">> [INGEST] Progress: {start_index + processed_in_chunk} / {len(all_files)}")

if __name__ == "__main__":
    main()
