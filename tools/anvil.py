#!/usr/bin/env python3
import sys
import json
import subprocess
import os
import shutil

# Anvil Sovereign Build Tool
# Implements the 'anvil build' command as per DOCS/anvil_coding_manual.md

PROJECT_ROOT = os.getcwd()
MPY_CROSS = os.path.join(PROJECT_ROOT, "oss_sovereignty_staging/sys_09_Anvil/source/mpy-cross/build/mpy-cross")
SOVEREIGN_GCC = os.path.join(PROJECT_ROOT, "toolchain/bin/x86_64-unknown-linux-musl-gcc")

def log(msg):
    print(f">> [ANVIL] {msg}")

def build(mpy_path, output_path):
    if not os.path.exists(mpy_path):
        print(f"Error: Source file {mpy_path} not found.")
        sys.exit(1)
        
    with open(mpy_path, 'r') as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError:
            print(f"Error: {mpy_path} is not a valid MicroJSON (.mpy) file.")
            sys.exit(1)
            
    source = data.get('human_readable_source', '')
    module_name = data.get('module_name', 'unknown')
    
    # Create temp file with correct extension
    ext = os.path.splitext(module_name)[1]
    temp_src = mpy_path + ".temp" + ext
    
    with open(temp_src, 'w') as f:
        f.write(source)

    try:
        # Determine language/type
        if module_name.endswith('.c') or module_name.endswith('.h'):
            log(f"Compiling C Source: {module_name} -> {output_path}")
            
            # STAGING FIX: Map sovereign repo path to staging path for headers
            # oss_sovereignty/foo/bar.mpy -> oss_sovereignty_staging/foo/
            
            base_dir = os.path.dirname(os.path.abspath(mpy_path))
            project_root_abs = os.path.abspath(PROJECT_ROOT)
            rel_path = os.path.relpath(base_dir, project_root_abs)
            
            if rel_path.startswith("oss_sovereignty"):
                staging_rel = rel_path.replace("oss_sovereignty", "oss_sovereignty_staging", 1)
                include_dir = os.path.join(PROJECT_ROOT, staging_rel)
            else:
                include_dir = base_dir

            # Also explicitly add bash-5.2.21 root if we are in it (common include pattern)
            # This is still a bit hacky but better than before
            cmd = [SOVEREIGN_GCC, "-c", temp_src, "-o", output_path, "-I", include_dir]
            
            if "bash-5.2.21" in include_dir:
                 bash_root = include_dir.split("bash-5.2.21")[0] + "bash-5.2.21"
                 cmd.extend(["-I", bash_root])
                 
                 # Ensure config.h exists in staging
                 config_h = os.path.join(bash_root, "config.h")
                 if not os.path.exists(config_h):
                      with open(config_h, 'w') as f: f.write("#define HAVE_UNISTD_H 1\n#define _POSIX_VERSION 200809L\n")

            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"Compilation Failed for {module_name}:\n{result.stderr}")
                sys.exit(1)

        elif module_name.endswith('.py'):
            log(f"Compiling Python Source: {module_name} -> {output_path}")
            cmd = [MPY_CROSS, "-o", output_path, temp_src]
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"Compilation Failed:\n{result.stderr}")
                sys.exit(1)
        
        else:
            # Fallback for Shell Scripts / Configs (Treat as 'Binary' blob for now)
            log(f"Encapsulating Asset: {module_name} -> {output_path}")
            # Just copy the content effectively (or compile to a wrapper if we had one)
            # For now, we write the raw content as the 'anv' artifact.
            # In the future, this might be a self-extracting binary or wrapped script.
            shutil.copy(temp_src, output_path)

    finally:
        if os.path.exists(temp_src):
            os.remove(temp_src)

def main():
    if len(sys.argv) < 2:
        print("Usage: anvil <command> [args]")
        sys.exit(1)
        
    command = sys.argv[1]
    
    if command == "build":
        # anvil build <source.mpy> -o <output.anv>
        if len(sys.argv) != 5 or sys.argv[3] != "-o":
            print("Usage: anvil build <source.mpy> -o <output.anv>")
            sys.exit(1)
        build(sys.argv[2], sys.argv[4])
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)

if __name__ == "__main__":
    main()
