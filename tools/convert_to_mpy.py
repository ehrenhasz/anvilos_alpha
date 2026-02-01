#!/usr/bin/env python3
import os
import json
import hashlib
import sys

def convert_file(file_path, output_root):
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        
        file_hash = hashlib.sha256(content.encode('utf-8')).hexdigest()
        filename = os.path.basename(file_path)
        
        # Structure defined in Anvil Coding Manual
        data = {
            "hash_id": file_hash,
            "module_name": filename,
            "original_prompt": "Imported from oss_sovereignty Phase 0.7",
            "human_readable_source": content,
            "logic_map": {},
            "failure_modes": [],
            "crash_correlation_map": {}
        }
        
        # Replicate directory structure in output
        rel_path = os.path.relpath(file_path, "oss_sovereignty")
        output_path = os.path.join(output_root, rel_path + ".mpy")
        
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
            
        return output_path
    except Exception as e:
        print(f"Failed to convert {file_path}: {e}")
        return None

def main():
    if len(sys.argv) < 3:
        print("Usage: convert_to_mpy.py <input_file_list> <output_dir>")
        sys.exit(1)
        
    input_list_file = sys.argv[1]
    output_dir = sys.argv[2]
    
    with open(input_list_file, 'r') as f:
        files = [line.strip() for line in f if line.strip()]
        
    print(f"Converting {len(files)} files to MicroJSON (.mpy)...")
    
    for file_path in files:
        out = convert_file(file_path, output_dir)
        if out:
            print(f"Created: {out}")

if __name__ == "__main__":
    main()
