#!/usr/bin/env python3
import os
import sys
import json
import shutil
import subprocess
import time
from datetime import datetime

# Get the absolute path to the project root
# The script is in system/scripts/, so we go up two levels
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

LOG_FILE = os.path.join(PROJECT_ROOT, "ext", "forge.log")

def log(message):
    timestamp = datetime.now().strftime('%Y-%m-%dT%H:%M:%S.%f')[:-3] + 'Z'
    with open(LOG_FILE, "a") as f:
        f.write(f"{timestamp} [assimilate] {message}\n")
    print(message)

def run_command(cmd, cwd=None):
    log(f"EXEC: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        log(f"ERROR: {result.stderr}")
        return False, result.stderr
    return True, result.stdout

def assimilate(target_path, repo_url):
    log(f">> ASSIMILATING: {target_path} FROM {repo_url}")
    
    if not os.path.exists(target_path):
        os.makedirs(target_path)
    
    source_dir = os.path.join(target_path, "source")
    
    # 1. Clone
    if os.path.exists(source_dir):
        log(f"!! Source dir exists at {source_dir}. Skipping clone (assuming manual placement or previous run).")
    else:
        log(f">> Cloning {repo_url}...")
        success, _ = run_command(f"git clone --depth 1 {repo_url} {source_dir}")
        if not success:
            return False

    # 2. Capture Commit Info (before killing .git)
    commit_hash = "unknown"
    if os.path.exists(os.path.join(source_dir, ".git")):
        success, out = run_command("git rev-parse HEAD", cwd=source_dir)
        if success:
            commit_hash = out.strip()
            
        # 3. Kill .git
        log(">> Severing connection (removing .git)...")
        shutil.rmtree(os.path.join(source_dir, ".git"))
    else:
        log("!! No .git found in source. Already severed?")

    # 4. Mark / Create Metadata
    meta_path = os.path.join(target_path, "metadata.json")
    metadata = {}
    if os.path.exists(meta_path):
        with open(meta_path, "r") as f:
            metadata = json.load(f)
            
    metadata.update({
        "sovereignty_level": "assimilated",
        "assimilated_at": datetime.now().isoformat(),
        "origin": {
            "url": repo_url,
            "commit": commit_hash,
            "policy": "monitor_only"
        },
        "mandate": "NEVER APPLY UPDATES AUTOMATICALLY. REWRITE IN ANVIL."
    })
    
    with open(meta_path, "w") as f:
        json.dump(metadata, f, indent=2)
        
    log(f">> Assimilation Complete. Metadata saved to {meta_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: assimilate.py <target_folder> <repo_url>")
        sys.exit(1)
        
    target = sys.argv[1]
    url = sys.argv[2]
    
    if not assimilate(target, url):
        sys.exit(1)
