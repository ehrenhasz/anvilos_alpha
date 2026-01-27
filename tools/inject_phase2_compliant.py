import sys
import os
sys.path.append("src")
from anvilos.mainframe_client import MainframeClient

MAINFRAME = MainframeClient("data/cortex.db")

CARDS = [
    # --- 1. SOURCE PREPARATION (The Foundation) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://github.com/openzfs/zfs/releases/download/zfs-2.2.2/zfs-2.2.2.tar.gz && tar -xf zfs-2.2.2.tar.gz && wget https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.39/util-linux-2.39.3.tar.xz && tar -xf util-linux-2.39.3.tar.xz && wget https://zlib.net/zlib-1.3.1.tar.gz && tar -xf zlib-1.3.1.tar.gz && wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.14.tar.xz && tar -xf linux-6.6.14.tar.xz",
            "timeout": 900
        },
        "desc": "Card 100: fetch_sources (zfs, util-linux, zlib, linux)"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "make -C oss_sovereignty/linux-6.6.14 mrproper",
            "timeout": 300
        },
        "desc": "Card 101: clean_linux_source"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "make -C oss_sovereignty/busybox-1.36.1 mrproper",
            "timeout": 300
        },
        "desc": "Card 102: clean_busybox_source"
    },

    # --- 2. USERLAND FORGING (The Muscles) ---
    # Busybox
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make defconfig && sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config && sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config",
            "timeout": 600
        },
        "desc": "Card 200: config_busybox_static"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make -j$(nproc)",
            "timeout": 600
        },
        "desc": "Card 201: build_busybox"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/rootfs_stage/bin && cp oss_sovereignty/busybox-1.36.1/busybox build_artifacts/rootfs_stage/bin/ && cd build_artifacts/rootfs_stage/bin && ./busybox --install .",
            "timeout": 60
        },
        "desc": "Card 202: install_busybox"
    },

    # ZFS Utils (Static Chain: zlib -> util-linux -> zfs)
    {
        "op": "SYS_CMD",
        "pld": {
            # Build Static Zlib
            "cmd": "cd oss_sovereignty/zlib-1.3.1 && ./configure --static --prefix=$(pwd)/../staging && make -j$(nproc) && make install",
            "timeout": 300
        },
        "desc": "Card 210a: build_static_zlib"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            # Build Static libuuid & libblkid
            "cmd": "cd oss_sovereignty/util-linux-2.39.3 && ./configure --prefix=$(pwd)/../staging --disable-all-programs --enable-libuuid --enable-libblkid --enable-static && make -j$(nproc) && make install",
            "timeout": 600
        },
        "desc": "Card 210b: build_static_util_linux"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && sed -i 's/\\bhighbit64\\b/zfs_highbit64_local/g' cmd/zstream/zstream_redup.c && sed -i 's/\\bsfread\\b/sfread_local/g' cmd/zstream/zstream_redup.c && sed -i 's/\\bzstream_do_redup\\b/zstream_do_redup_local/g' cmd/zstream/zstream_redup.c && export PKG_CONFIG_PATH=../staging/lib/pkgconfig && ./configure --with-config=user --enable-static=yes --enable-shared=no --prefix=/ --libdir=/lib --sbindir=/sbin",
            "timeout": 600
        },
        "desc": "Card 211: config_zfs_static"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "Card 212: build_zfs_utils"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/rootfs_stage/sbin && cp oss_sovereignty/zfs-2.2.2/cmd/zpool/zpool build_artifacts/rootfs_stage/sbin/ && cp oss_sovereignty/zfs-2.2.2/cmd/zfs/zfs build_artifacts/rootfs_stage/sbin/",
            "timeout": 60
        },
        "desc": "Card 213: install_zfs_utils"
    },

    # --- 3. KERNEL FORGING (The Heart) ---
    {
        "op": "SYS_CMD",
        "pld": {
            # Prepare kernel headers/env for module build
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make defconfig && make modules_prepare",
            "timeout": 600
        },
        "desc": "Card 300: prepare_kernel_headers"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            # Build ZFS modules against kernel source
            "cmd": "cd oss_sovereignty/zfs-2.2.2 && ./configure --with-config=kernel --with-linux=../linux-6.6.14 --with-linux-obj=../linux-6.6.14 && make -j$(nproc)",
            "timeout": 1200
        },
        "desc": "Card 301: graft_zfs_modules (build)"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            # Configure kernel for Monolith/Initrd support
            "cmd": "cd oss_sovereignty/linux-6.6.14 && sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && make olddefconfig",
            "timeout": 300
        },
        "desc": "Card 302: config_kernel_monolith"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make -j$(nproc) bzImage",
            "timeout": 1800
        },
        "desc": "Card 303: build_kernel_bzImage"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make modules",
            "timeout": 1800
        },
        "desc": "Card 304: build_kernel_modules"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/iso/boot && cp oss_sovereignty/linux-6.6.14/arch/x86/boot/bzImage build_artifacts/iso/boot/",
            "timeout": 60
        },
        "desc": "Card 305: install_kernel_artifacts"
    },

    # --- 4. THE ZFS ROOT (The Body) ---
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "dd if=/dev/zero of=build_artifacts/rootfs.img bs=1M count=500",
            "timeout": 60
        },
        "desc": "Card 400: create_sparse_rootfs"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            # Attempting pool creation. This requires privilege.
            "cmd": "./build_artifacts/rootfs_stage/sbin/zpool create -o ashift=12 -O compression=lz4 -m legacy -f zroot $(pwd)/build_artifacts/rootfs.img && ./build_artifacts/rootfs_stage/sbin/zpool export zroot",
            "timeout": 120
        },
        "desc": "Card 401: format_rootfs_pool"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            # Import, populate, export.
            "cmd": "./build_artifacts/rootfs_stage/sbin/zpool import -d $(pwd)/build_artifacts/rootfs.img zroot && mkdir -p /tmp/zroot_mnt && mount -t zfs zroot /tmp/zroot_mnt && cp -a build_artifacts/rootfs_stage/* /tmp/zroot_mnt/ && umount /tmp/zroot_mnt && ./build_artifacts/rootfs_stage/sbin/zpool export zroot",
            "timeout": 300
        },
        "desc": "Card 402: mount_and_populate"
    },

    # --- 5. THE INITRAMFS (The Key) ---
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/rootfs_stage/init",
            "content": "#!/bin/busybox sh\n\nmount -t proc none /proc\nmount -t sysfs none /sys\nmknod /dev/null c 1 3\nmknod /dev/console c 5 1\n\necho 'Loading ZFS Modules...'\nfind /lib/modules -name \"*.ko\" -exec insmod {} \\;\n\necho 'Importing ZRoot...'\nzpool import -d /boot/rootfs.img zroot\nmount -t zfs zroot /new_root\n\necho 'Switching Root...'\nexec switch_root /new_root /bin/sh\n"
        },
        "desc": "Card 500: write_init_script"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "chmod +x build_artifacts/rootfs_stage/init && cd build_artifacts/rootfs_stage && find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz",
            "timeout": 60
        },
        "desc": "Card 501: pack_initramfs"
    },

    # --- 6. FINAL ARTIFACT (The Anvil) ---
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/iso/boot/isolinux/isolinux.cfg",
            "content": "DEFAULT anvilos\nLABEL anvilos\n  KERNEL /boot/bzImage\n  INITRD /boot/initramfs.cpio.gz\n  APPEND console=ttyS0 quiet\n"
        },
        "desc": "Card 600: generate_isolinux_cfg"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/iso/boot/isolinux && cp build_artifacts/initramfs.cpio.gz build_artifacts/iso/boot/ && cp build_artifacts/rootfs.img build_artifacts/iso/boot/ && xorriso -as mkisofs -o build_artifacts/anvilos-phase2.iso -b boot/isolinux/isolinux.bin -c boot/isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table build_artifacts/iso",
            "timeout": 120
        },
        "desc": "Card 601: burn_iso"
    }
]

print(f"[*] Injecting {len(CARDS)} Phase 2 (Compliant - V2 Static Chain) Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")