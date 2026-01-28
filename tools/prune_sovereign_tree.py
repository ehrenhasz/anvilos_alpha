#!/usr/bin/env python3
import os
import shutil
import glob

# Paths
ROOT = os.getcwd()
LINUX_DIR = os.path.join(ROOT, 'oss_sovereignty', 'linux-6.6.14')
ZFS_DIR = os.path.join(ROOT, 'oss_sovereignty', 'zfs')
BUSYBOX_DIR = os.path.join(ROOT, 'oss_sovereignty', 'busybox')

def prune_dir(path):
    if os.path.exists(path):
        print(f"Removing: {path}")
        shutil.rmtree(path)
    else:
        # Check for glob patterns
        for p in glob.glob(path):
            if os.path.isdir(p):
                print(f"Removing: {p}")
                shutil.rmtree(p)
            else:
                print(f"Removing file: {p}")
                os.remove(p)

def prune_linux():
    print("--- Pruning Linux Kernel ---")
    
    # 1. ARCHITECTURES (Keep x86)
    arch_dir = os.path.join(LINUX_DIR, 'arch')
    for d in os.listdir(arch_dir):
        full_path = os.path.join(arch_dir, d)
        if d == 'x86' or d == 'Kconfig':
            continue
        if os.path.isfile(full_path):
            print(f"Removing file: {full_path}")
            os.remove(full_path)
        else:
            prune_dir(full_path)
        
    # 2. DOCUMENTATION
    prune_dir(os.path.join(LINUX_DIR, 'Documentation'))
    
    # 3. BLOAT DIRS
    prune_dir(os.path.join(LINUX_DIR, 'samples'))
    prune_dir(os.path.join(LINUX_DIR, 'tools')) # Usually for host tools, but minimal build might skip
    prune_dir(os.path.join(LINUX_DIR, 'sound')) # No audio in minimal
    
    # 4. DRIVERS (Aggressive)
    # RFC-2026-000010: "Drivers: Explicitly enabled ONLY"
    # Removing vast source trees for hardware we don't support reduces attack surface & file count.
    drivers_dir = os.path.join(LINUX_DIR, 'drivers')
    
    # Remove GPU drivers (Headless/Server)
    prune_dir(os.path.join(drivers_dir, 'gpu'))
    
    # Remove Wireless (Hardline only)
    prune_dir(os.path.join(drivers_dir, 'net', 'wireless'))
    prune_dir(os.path.join(drivers_dir, 'wireless')) # top level wireless subsystem
    
    # Remove Staging
    prune_dir(os.path.join(drivers_dir, 'staging'))
    
    # Remove Infiniband
    prune_dir(os.path.join(drivers_dir, 'infiniband'))
    
    # Remove ISDN
    prune_dir(os.path.join(drivers_dir, 'isdn'))
    
    # Remove Media (Webcams, TV tuners)
    prune_dir(os.path.join(drivers_dir, 'media'))

def prune_zfs():
    print("--- Pruning ZFS ---")
    # ZFS Tests are massive
    prune_dir(os.path.join(ZFS_DIR, 'tests'))
    # ZFS man pages source (we might need them for install, but maybe we can skip?)
    # Manifest says "Man pages for INSTALLED tools only". 
    # Usually we build man pages from source. Let's keep man for now, pruning tests is the big win.

def prune_busybox():
    print("--- Pruning Busybox ---")
    prune_dir(os.path.join(BUSYBOX_DIR, 'docs'))
    prune_dir(os.path.join(BUSYBOX_DIR, 'examples'))
    # Arch specific in busybox is usually minimal or handled by config, 
    # but check for 'arch' folder if it exists (usually doesn't in root).

def count_files(directory):
    count = 0
    for root, dirs, files in os.walk(directory):
        count += len(files)
    return count

def main():
    print("Starting Sovereign Tree Pruning (RFC-2026-000010)...")
    
    if os.path.exists(LINUX_DIR):
        prune_linux()
    
    if os.path.exists(ZFS_DIR):
        prune_zfs()
        
    if os.path.exists(BUSYBOX_DIR):
        prune_busybox()
        
    total_files = 0
    if os.path.exists(LINUX_DIR): total_files += count_files(LINUX_DIR)
    if os.path.exists(ZFS_DIR): total_files += count_files(ZFS_DIR)
    if os.path.exists(BUSYBOX_DIR): total_files += count_files(BUSYBOX_DIR)
    
    print(f"\nTotal Files Remaining in oss_sovereignty: {total_files}")

if __name__ == "__main__":
    main()
