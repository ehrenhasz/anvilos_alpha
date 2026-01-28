#!/usr/bin/env python3
import os
import sys

# Add project root to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from forge_directives import PHASE2_DIRECTIVES
except ImportError:
    print("Error: Could not import PHASE2_DIRECTIVES from forge_directives.py")
    sys.exit(1)

def prune():
    print("Building whitelist from forge_directives.py...")
    whitelist = set()
    for item in PHASE2_DIRECTIVES:
        if item.get('op') == 'verify_file_integrity':
            # Check payload
            pld = item.get('pld', {})
            path = pld.get('path')
            if path:
                # Normalize and ensure it starts with oss_sovereignty
                norm = os.path.normpath(path)
                if norm.startswith("oss_sovereignty"):
                    whitelist.add(norm)
    
    print(f"Whitelist contains {len(whitelist)} files.")
    
    target_root = "oss_sovereignty"
    if not os.path.exists(target_root):
        print("oss_sovereignty not found!")
        return

    deleted = 0
    kept = 0
    
    print(f"Scanning {target_root}...")
    for root, dirs, files in os.walk(target_root, topdown=False):
        for name in files:
            full_path = os.path.join(root, name)
            rel_path = os.path.normpath(full_path)
            
            if rel_path not in whitelist:
                # Delete
                try:
                    os.remove(full_path)
                    deleted += 1
                    if deleted % 1000 == 0:
                        print(f"Deleted {deleted}...", end='\r')
                except OSError as e:
                    print(f"Failed to delete {full_path}: {e}")
            else:
                kept += 1
        
        # Prune empty dirs
        if not os.listdir(root):
            try:
                os.rmdir(root)
            except OSError as e:
                pass
                
    print(f"\nFinal Report:")
    print(f"Files Kept: {kept}")
    print(f"Files Deleted: {deleted}")

if __name__ == "__main__":
    prune()
