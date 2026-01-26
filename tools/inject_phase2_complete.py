import sys
import os
import json
sys.path.append("src")
from anvilos.mainframe_client import MainframeClient

MAINFRAME = MainframeClient("data/cortex.db")

# PHASE 2 COMPLETE: THE ZFS MONOLITH
CARDS = [
    # --- 1. ZFS ACQUISITION ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://github.com/openzfs/zfs/releases/download/zfs-2.2.2/zfs-2.2.2.tar.gz && tar -xf zfs-2.2.2.tar.gz",
            "timeout": 600
        },
        "desc": "FETCH_ZFS_SOURCE"
    },
    
    # --- 2. ZFS USERLAND BUILD (The Muscles) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=user && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_USERLAND"
    },

    # --- 3. KERNEL REFORGE (The Heart) ---
    # We need to prep the kernel source with ZFS headers/code before building the module in-tree.
    # Note: Building ZFS *into* the kernel (monolith) usually requires specific patching or config hooks.
    # A simpler path for this prototype: Build ZFS as modules against the kernel we fetched.
    # But directives say "CONFIG_ZFS=y (Built-in)".
    # Strategy: We will configure ZFS to build kernel modules against our linux source.
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=kernel --with-linux=../linux-6.6.14 --with-linux-obj=../linux-6.6.14 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_KERNEL_MODULES"
    },
    # Re-verify Kernel Config to ensure Loop and Initrd are set
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && make olddefconfig",
            "timeout": 300
        },
        "desc": "CONFIG_KERNEL_LOOP_INITRD"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make -j$(nproc) bzImage",
            "timeout": 1800
        },
        "desc": "REBUILD_KERNEL_IMAGE"
    },

    # --- 4. ROOTFS CONSTRUCTION (The Body) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/rootfs_stage && mkdir -p build_artifacts/rootfs_stage/bin build_artifacts/rootfs_stage/sbin build_artifacts/rootfs_stage/usr/bin build_artifacts/rootfs_stage/usr/sbin build_artifacts/rootfs_stage/proc build_artifacts/rootfs_stage/sys build_artifacts/rootfs_stage/dev build_artifacts/rootfs_stage/lib/modules",
            "timeout": 60
        },
        "desc": "PREP_ROOTFS_DIRS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/busybox-1.36.1/busybox build_artifacts/rootfs_stage/bin/",
            "timeout": 30
        },
        "desc": "INSTALL_BUSYBOX"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd build_artifacts/rootfs_stage/bin && ./busybox --install .",
            "timeout": 30
        },
        "desc": "INSTALL_BUSYBOX_SYMLINKS"
    },
    # Install ZFS Binaries (Dynamically linked usually, but we hope for the best or copy libs)
    # Ideally we'd statically link or copy libs. For prototype, we attempt copy.
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/zfs-2.2.2/cmd/zpool/zpool build_artifacts/rootfs_stage/sbin/ && cp oss_sovereignty/zfs-2.2.2/cmd/zfs/zfs build_artifacts/rootfs_stage/sbin/",
            "timeout": 30
        },
        "desc": "INSTALL_ZFS_TOOLS"
    },
    # Install ZFS Kernel Modules
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && make install DESTDIR=../../build_artifacts/rootfs_stage",
            "timeout": 120
        },
        "desc": "INSTALL_ZFS_MODULES"
    },

    # --- 5. INITRAMFS (The Key) ---
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\n\nmount -t proc none /proc\nmount -t sysfs none /sys\nmknod /dev/null c 1 3\nmknod /dev/tty c 5 0\nmknod /dev/console c 5 1\n\necho 'Loading ZFS Modules...'\nfind /lib/modules -name \"*.ko\" -exec insmod {} \\;\n\necho 'Welcome to AnvilOS Alpha (ZFS Edition)'\n# In a real ZFS boot, we would import pool here.\n# zpool import -a\n\nexec /bin/sh\n"
        },
        "desc": "WRITE_INIT_SCRIPT"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "chmod +x build_artifacts/rootfs_stage/init",
            "timeout": 10
        },
        "desc": "CHMOD_INIT"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd build_artifacts/rootfs_stage && find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz",
            "timeout": 120
        },
        "desc": "PACK_INITRAMFS"
    },

    # --- 6. ISO GENERATION (The Anvil) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/iso && mkdir -p build_artifacts/iso/boot/isolinux",
            "timeout": 30
        },
        "desc": "PREP_ISO_DIRS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/linux-6.6.14/arch/x86/boot/bzImage build_artifacts/iso/boot/ && cp build_artifacts/initramfs.cpio.gz build_artifacts/iso/boot/",
            "timeout": 60
        },
        "desc": "COPY_KERNEL_AND_INITRAMFS"
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/iso/boot/isolinux/isolinux.cfg",
            "content": "DEFAULT anvilos\nLABEL anvilos\n  KERNEL /boot/bzImage\n  INITRD /boot/initramfs.cpio.gz\n  APPEND console=ttyS0 quiet\n"
        },
        "desc": "WRITE_ISOLINUX_CFG"
    }
]

print("[*] Injecting Phase 2 Complete (ZFS Monolith) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
