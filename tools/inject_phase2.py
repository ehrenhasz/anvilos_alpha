import sys
import os
import json
sys.path.append("src")
from anvilos.mainframe_client import MainframeClient
MAINFRAME = MainframeClient("data/cortex.db")
CARDS = [
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://github.com/openzfs/zfs/releases/download/zfs-2.2.2/zfs-2.2.2.tar.gz && tar -xf zfs-2.2.2.tar.gz",
            "timeout": 600
        },
        "desc": "FETCH_ZFS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=user && make -j2",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_USERLAND"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts && dd if=/dev/zero of=build_artifacts/rootfs.img bs=1M count=500",
            "timeout": 60
        },
        "desc": "CREATE_ROOTFS_IMG"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/rootfs_stage/bin build_artifacts/rootfs_stage/sbin build_artifacts/rootfs_stage/proc build_artifacts/rootfs_stage/sys build_artifacts/rootfs_stage/dev",
            "timeout": 30
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
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\n\nmount -t proc none /proc\nmount -t sysfs none /sys\n\necho 'Welcome to AnvilOS Alpha'\nexec /bin/sh\n"
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
            "timeout": 60
        },
        "desc": "PACK_INITRAMFS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/iso/boot/isolinux",
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
    },
]
print("[*] Injecting Phase 2 (Part 2) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
