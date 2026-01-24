import os
import sys
import subprocess
import time
import shutil

# CONFIG
# Target: The directory the Wine DTRPG app watches/downloads to
TARGET_DIR = os.path.expanduser("~/Documents/DriveThruRPG")
# Tablet Source: Typical download location
TABLET_SOURCE = "/sdcard/Download"

def check_adb():
    """Checks if a device is connected."""
    try:
        res = subprocess.run(["adb", "devices"], capture_output=True, text=True)
        lines = res.stdout.strip().split('\n')[1:] # Skip header
        devices = [l for l in lines if l.strip() and "device" in l]
        return len(devices) > 0
    except:
        return False

def pull_files():
    if not check_adb():
        print("[Tablet] No device found. Please plug in Pixel Tablet and enable USB Debugging.")
        return

    print("[Tablet] Device detected. Scanning for PDFs/ZIPs in Download folder...")
    
    # List files on device
    cmd = ["adb", "shell", "ls", TABLET_SOURCE]
    res = subprocess.run(cmd, capture_output=True, text=True)
    
    if res.returncode != 0:
        print(f"[Tablet] Error listing files: {res.stderr}")
        return

    files = res.stdout.strip().split('\n')
    candidates = [f.strip() for f in files if f.lower().endswith(('.pdf', '.zip'))]
    
    if not candidates:
        print("[Tablet] No PDFs or ZIPs found in /sdcard/Download.")
        return

    print(f"[Tablet] Found {len(candidates)} candidate files.")
    os.makedirs(TARGET_DIR, exist_ok=True)
    
    count = 0
    for filename in candidates:
        # Check if we already have it to avoid redundant transfer
        local_path = os.path.join(TARGET_DIR, filename)
        if os.path.exists(local_path):
            print(f"[Tablet] Skipping {filename} (Already exists locally)")
            continue
            
        print(f"[Tablet] Pulling {filename}...")
        remote_path = f"{TABLET_SOURCE}/{filename}"
        pull_cmd = ["adb", "pull", remote_path, local_path]
        
        pull_res = subprocess.run(pull_cmd, capture_output=True, text=True)
        if pull_res.returncode == 0:
            print(f"[Tablet] Successfully imported: {filename}")
            count += 1
        else:
            print(f"[Tablet] Failed to pull {filename}: {pull_res.stderr}")

    print(f"[Tablet] Ingest complete. Imported {count} new files into {TARGET_DIR}")

if __name__ == "__main__":
    pull_files()
