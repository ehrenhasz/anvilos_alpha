import sys
import os
import json
sys.path.append("src")
from anvilos.mainframe_client import MainframeClient

MAINFRAME = MainframeClient("data/cortex.db")

CARDS = [
    # --- 1. CLEANUP ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/rootfs_stage build_artifacts/iso build_artifacts/bin build_artifacts/initramfs.cpio.gz oss_sovereignty/staging",
            "timeout": 60
        },
        "desc": "CLEAN_ARTIFACTS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "make -C oss_sovereignty/linux-6.6.14 distclean && make -C oss_sovereignty/busybox-1.36.1 distclean",
            "timeout": 300
        },
        "desc": "CLEAN_BUILD_TREES"
    },

    # --- 2. FETCH & BUILD ZLIB (Static) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://zlib.net/zlib-1.3.1.tar.gz && tar -xf zlib-1.3.1.tar.gz && cd zlib-1.3.1 && ./configure --static --prefix=$(pwd)/../staging && make -j$(nproc) && make install",
            "timeout": 300
        },
        "desc": "BUILD_ZLIB"
    },

    # --- 3. BUILD LIBUUID & LIBBLKID (Static) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/util-linux-2.39.3 && ./configure --prefix=$(pwd)/../staging --disable-all-programs --enable-libuuid --enable-libblkid --enable-static --disable-shared && make -j$(nproc) && make install",
            "timeout": 600
        },
        "desc": "BUILD_UTIL_LINUX_LIBS"
    },

    # --- 4. BUILD ZFS USERLAND (With Explicit Flags) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && export STAGING=$(pwd)/../staging && ./configure --with-config=user --enable-static=yes --enable-shared=no --prefix=/ --libdir=/lib --sbindir=/sbin --disable-systemd --disable-pyzfs --with-udevdir=no --disable-nls LIBUUID_CFLAGS=\"-I$STAGING/include\" LIBUUID_LIBS=\"-L$STAGING/lib -luuid\" LIBBLKID_CFLAGS=\"-I$STAGING/include\" LIBBLKID_LIBS=\"-L$STAGING/lib -lblkid\" ZLIB_CFLAGS=\"-I$STAGING/include\" ZLIB_LIBS=\"-L$STAGING/lib -lz\" && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_USERLAND"
    },

    # --- 5. PREP KERNEL ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make defconfig && sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && make olddefconfig && make modules_prepare",
            "timeout": 600
        },
        "desc": "PREP_KERNEL"
    },

    # --- 6. BUILD ZFS MODULES ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=kernel --with-linux=../linux-6.6.14 --with-linux-obj=../linux-6.6.14 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_MODULES"
    },

    # --- 7. BUILD BUSYBOX (FIXED) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make defconfig && sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config && sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config && sed -i 's/CONFIG_FEATURE_TC_INGRESS=y/# CONFIG_FEATURE_TC_INGRESS is not set/' .config && make -j$(nproc)",
            "timeout": 600
        },
        "desc": "BUILD_BUSYBOX"
    },

    # --- 8. ROOTFS & ISO ---
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
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make -j$(nproc) bzImage",
            "timeout": 1800
        },
        "desc": "BUILD_KERNEL_IMAGE"
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\nmount -t proc none /proc\nmount -t sysfs none /sys\nmknod /dev/null c 1 3\nmknod /dev/console c 5 1\necho 'Loading ZFS...'\nfind /lib/modules -name \"*.ko\" -exec insmod {} \\\\;\necho 'AnvilOS Alpha'\nexec /bin/sh\n"
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
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/iso/boot/isolinux && cp oss_sovereignty/linux-6.6.14/arch/x86/boot/bzImage build_artifacts/iso/boot/ && cp build_artifacts/initramfs.cpio.gz build_artifacts/iso/boot/",
            "timeout": 60
        },
        "desc": "PREP_ISO"
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/iso/boot/isolinux/isolinux.cfg",
            "content": "DEFAULT anvilos\nLABEL anvilos\n  KERNEL /boot/bzImage\n  INITRD /boot/initramfs.cpio.gz\n  APPEND console=ttyS0 quiet\n"
        },
        "desc": "WRITE_ISOLINUX_CFG"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "xorriso -as mkisofs -o build_artifacts/anvilos-alpha.iso -b boot/isolinux/isolinux.bin -c boot/isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table build_artifacts/iso",
            "timeout": 120
        },
        "desc": "BURN_ISO"
    }
]

print(f"[*] Injecting {len(CARDS)} Phase 2 (FIXED V2) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
