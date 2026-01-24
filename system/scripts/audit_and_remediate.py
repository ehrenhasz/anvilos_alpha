#!/usr/bin/env python3
import os
import json
import subprocess
import sys
from datetime import datetime

# Path setup
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
ASSIMILATE_SCRIPT = os.path.join(SCRIPT_DIR, "assimilate.py")
TARGET_ROOT = os.path.join(PROJECT_ROOT, "oss_sovereignty")

def log(message):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}")

def audit_and_remediate(root_dir):
    log(f"Starting audit of {root_dir}...")
    
    remediated_count = 0
    skipped_count = 0
    
    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Check if this is a "leaf" or system directory we care about
        # We look for metadata.json to confirm it's a tracked entity
        if "metadata.json" in filenames:
            meta_path = os.path.join(dirpath, "metadata.json")
            source_path = os.path.join(dirpath, "source")
            
            try:
                with open(meta_path, "r") as f:
                    meta = json.load(f)
            except json.JSONDecodeError:
                log(f"ERROR: Corrupt metadata at {meta_path}")
                continue
                
            # Check if source exists
            if not os.path.exists(source_path):
                log(f"MISSING SOURCE: {dirpath}")
                
                # Try to get URL
                url = meta.get("origin", {}).get("url")
                if not url:
                    # Fallback check for flat structure if older schema
                    url = meta.get("url")
                
                if url:
                    log(f"found URL: {url} -> REMEDIATING...")
                    # Call assimilate.py
                    cmd = [sys.executable, ASSIMILATE_SCRIPT, dirpath, url]
                    proc = subprocess.run(cmd, capture_output=True, text=True)
                    
                    if proc.returncode == 0:
                        log(f"SUCCESS: {dirpath} remediated.")
                        remediated_count += 1
                    else:
                        log(f"FAILURE: Could not assimilate {dirpath}.\nSTDERR: {proc.stderr}")
                else:
                    log(f"SKIPPED: No URL in metadata for {dirpath}")
                    skipped_count += 1
            else:
                # Source exists, check if it looks like a repo (has .git?)
                # If .git exists, we must sever it
                git_path = os.path.join(source_path, ".git")
                if os.path.exists(git_path):
                    log(f"SECURITY ALERT: .git found in {source_path}. Severing...")
                    # We can use assimilate to re-run (it handles existing dirs correctly)
                    # Or just remove it. Let's rely on assimilate logic which handles metadata updates too.
                    url = meta.get("origin", {}).get("url")
                    if url:
                         cmd = [sys.executable, ASSIMILATE_SCRIPT, dirpath, url]
                         subprocess.run(cmd, capture_output=True, text=True)
                         log("Severed.")
                         remediated_count += 1
                    else:
                        log(f"CANNOT SEVER: No URL to verify against in {meta_path}")
    
    log(f"Audit Complete. Remediated: {remediated_count}, Skipped: {skipped_count}")

if __name__ == "__main__":
    audit_and_remediate(TARGET_ROOT)
