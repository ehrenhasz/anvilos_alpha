#!/usr/bin/env python3
import os
import sys
import subprocess

# Add current directory to path to allow import
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)

try:
    from audit_coverage import SYSTEMS, REPO_MAP
    from assimilate import assimilate
except ImportError as e:
    print(f"Error importing modules: {e}")
    sys.exit(1)

PROJECT_ROOT = os.path.abspath(os.path.join(current_dir, '..', '..'))
SOVEREIGNTY_ROOT = os.path.join(PROJECT_ROOT, "oss_sovereignty")

def process_matrix():
    print(f">> STARTING MATRIX ASSIMILATION")
    
    for sys_name, sys_data in SYSTEMS.items():
        print(f">> SCANNING: {sys_name}")
        for cat_name, items in sys_data["categories"].items():
            for item_name in items:
                # Construct path
                target_path = os.path.join(SOVEREIGNTY_ROOT, sys_name, cat_name, item_name)
                
                # Check if we have a URL for this item
                repo_url = REPO_MAP.get(item_name)
                
                if not repo_url:
                    print(f"   [SKIP] No URL for {item_name}")
                    continue

                # Check if source exists
                source_path = os.path.join(target_path, "source")
                if os.path.exists(source_path) and os.listdir(source_path):
                    print(f"   [OK] {item_name} source present.")
                    continue
                
                # Assimilate
                print(f"   [MISSING] {item_name} source not found. Assimilating...")
                try:
                    success = assimilate(target_path, repo_url)
                    if success:
                        print(f"   [SUCCESS] Assimilated {item_name}")
                    else:
                        print(f"   [FAIL] Failed to assimilate {item_name}")
                except Exception as e:
                    print(f"   [ERROR] Exception assimilating {item_name}: {e}")

if __name__ == "__main__":
    process_matrix()
