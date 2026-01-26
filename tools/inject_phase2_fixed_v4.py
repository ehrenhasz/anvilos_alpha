import sys
import os
import json
sys.path.append("src")
from anvilos.mainframe_client import MainframeClient

MAINFRAME = MainframeClient("data/cortex.db")

CARDS = [
    # --- 1. PATCH ZFS (Fix Duplicate Symbol) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "sed -i 's/highbit64/zstream_highbit64/g' oss_sovereignty/zfs-2.2.2/cmd/zstream/zstream_redup.c",
            "timeout": 10
        },
        "desc": "PATCH_ZFS_ZSTREAM"
    },

    # --- 2. BUILD ZFS USERLAND (Retry) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && export STAGING=$(pwd)/../staging && ./configure --with-config=user --enable-static=yes --enable-shared=no --prefix=/ --libdir=/lib --sbindir=/sbin --disable-systemd --disable-pyzfs --with-udevdir=no --disable-nls LIBUUID_CFLAGS=\" -I$STAGING/include\" LIBUUID_LIBS=\" -L$STAGING/lib -luuid\" LIBBLKID_CFLAGS=\" -I$STAGING/include\" LIBBLKID_LIBS=\" -L$STAGING/lib -lblkid\" ZLIB_CFLAGS=\" -I$STAGING/include\" ZLIB_LIBS=\" -L$STAGING/lib -lz\" LIBTIRPC_CFLAGS=\" -I$STAGING/include/tirpc\" LIBTIRPC_LIBS=\" -L$STAGING/lib -ltirpc\" && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_USERLAND"
    },

    # --- 3. PREP KERNEL ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make defconfig && sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && make olddefconfig && make modules_prepare",
            "timeout": 600
        },
        "desc": "PREP_KERNEL"
    },

    # --- 4. BUILD ZFS MODULES ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=kernel --with-linux=../linux-6.6.14 --with-linux-obj=../linux-6.6.14 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_MODULES"
    },

    # --- 5. BUILD KERNEL IMAGE ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make -j$(nproc) bzImage",
            "timeout": 1800
        },
        "desc": "BUILD_KERNEL_IMAGE"
    },

    # --- 6. ROOTFS & INITRAMFS ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/rootfs_stage && mkdir -p build_artifacts/rootfs_stage/bin build_artifacts/rootfs_stage/sbin build_artifacts/rootfs_stage/lib/modules",
            "timeout": 30
        },
        "desc": "PREP_ROOTFS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/busybox-1.36.1/busybox build_artifacts/rootfs_stage/bin/ && cd build_artifacts/rootfs_stage/bin && ./busybox --install .",
            "timeout": 60
        },
        "desc": "INSTALL_BUSYBOX"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/zfs-2.2.2/cmd/zpool/zpool build_artifacts/rootfs_stage/sbin/ && cp oss_sovereignty/zfs-2.2.2/cmd/zfs/zfs build_artifacts/rootfs_stage/sbin/",
            "timeout": 30
        },
        "desc": "INSTALL_ZFS_TOOLS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && make install DESTDIR=../../build_artifacts/rootfs_stage",
            "timeout": 120
        },
        "desc": "INSTALL_ZFS_MODULES"
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\nmount -t proc none /proc\nmount -t sysfs none /sys\nmknod /dev/null c 1 3\nmknod /dev/console c 5 1\necho 'Loading ZFS...'\nfind /lib/modules -name \"*.ko\" -exec insmod {} \\;\necho 'AnvilOS Alpha Ready.'\nexec /bin/sh"
        },
        "desc": "WRITE_INIT"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "chmod +x build_artifacts/rootfs_stage/init && cd build_artifacts/rootfs_stage && find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz",
            "timeout": 60
        },
        "desc": "PACK_INITRAMFS"
    }
]

print(f"[*] Injecting {len(CARDS)} Phase 2 (FIXED V4) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
