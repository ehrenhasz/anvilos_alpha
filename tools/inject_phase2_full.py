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
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://github.com/openzfs/zfs/releases/download/zfs-2.2.2/zfs-2.2.2.tar.gz && tar -xf zfs-2.2.2.tar.gz && wget https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.39/util-linux-2.39.3.tar.xz && tar -xf util-linux-2.39.3.tar.xz",
            "timeout": 900
        },
        "desc": "FETCH_SOURCES"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "make -C oss_sovereignty/linux-6.6.14 distclean && make -C oss_sovereignty/busybox-1.36.1 distclean",
            "timeout": 300
        },
        "desc": "CLEAN_OLD_BUILDS"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/util-linux-2.39.3 && ./configure --prefix=$(pwd)/../staging --disable-all-programs --enable-libuuid --enable-static && make -j$(nproc) && make install",
            "timeout": 600
        },
        "desc": "BUILD_LIBUUID"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make defconfig && sed -i 's/CONFIG_EXT4_FS=y/# CONFIG_EXT4_FS is not set/' .config && sed -i 's/CONFIG_BTRFS_FS=y/# CONFIG_BTRFS_FS is not set/' .config && sed -i 's/CONFIG_XFS_FS=y/# CONFIG_XFS_FS is not set/' .config && sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && make olddefconfig && make modules_prepare",
            "timeout": 600
        },
        "desc": "PREP_KERNEL_SOURCE"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && export PKG_CONFIG_PATH=../staging/lib/pkgconfig && ./configure --with-config=user --enable-static=yes --enable-shared=no --prefix=/ --libdir=/lib --sbindir=/sbin && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_USERLAND"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=kernel --with-linux=../linux-6.6.14 --with-linux-obj=../linux-6.6.14 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "BUILD_ZFS_MODULES"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make defconfig && sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config && make -j$(nproc)",
            "timeout": 600
        },
        "desc": "BUILD_STATIC_BUSYBOX"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/rootfs_stage && mkdir -p build_artifacts/rootfs_stage/bin build_artifacts/rootfs_stage/sbin build_artifacts/rootfs_stage/lib/modules build_artifacts/rootfs_stage/proc build_artifacts/rootfs_stage/sys build_artifacts/rootfs_stage/dev",
            "timeout": 30
        },
        "desc": "PREP_ROOTFS_DIRS"
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
        "desc": "BUILD_BZIMAGE"
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\n\nmount -t proc none /proc\nmount -t sysfs none /sys\nmknod /dev/null c 1 3\nmknod /dev/console c 5 1\n\necho 'Loading ZFS Modules...'\nfind /lib/modules -name \"*.ko\" -exec insmod {} \\;\n\necho 'Welcome to AnvilOS Alpha'\nexec /bin/sh\n"
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
            "cmd": "rm -rf build_artifacts/iso && mkdir -p build_artifacts/iso/boot/isolinux",
            "timeout": 30
        },
        "desc": "PREP_ISO_DIR"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/linux-6.6.14/arch/x86/boot/bzImage build_artifacts/iso/boot/ && cp build_artifacts/initramfs.cpio.gz build_artifacts/iso/boot/",
            "timeout": 30
        },
        "desc": "COPY_KERNEL_INITRD"
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
print(f"[*] Injecting {len(CARDS)} Phase 2 (ZFS Monolith Fixed) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
