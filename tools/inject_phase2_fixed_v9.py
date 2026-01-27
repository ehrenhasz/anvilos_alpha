import sqlite3
import os
import json
import uuid

DB_PATH = "data/cortex.db"
PROJECT_ROOT = os.getcwd()
STAGING = f"{PROJECT_ROOT}/oss_sovereignty/staging"
KSRC = f"{PROJECT_ROOT}/oss_sovereignty/linux-6.6.14"
ZSRC = f"{PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2"

def inject():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("DELETE FROM card_stack")
    
    cards = [
        ("CLEAN_ENV", f"rm -rf {STAGING} && mkdir -p {STAGING}/lib {STAGING}/include"),

        ("BUILD_ZLIB", 
         f"cd {PROJECT_ROOT}/oss_sovereignty/zlib-1.3.1 && "
         f"./configure --prefix={STAGING} --static && "
         f"make -j$(nproc) install"),

        ("BUILD_UTIL_LINUX",
         f"cd {PROJECT_ROOT}/oss_sovereignty/util-linux-2.39.3 && "
         f"./configure --prefix={STAGING} --disable-all-programs --enable-libuuid --enable-libblkid --disable-bash-completion --disable-shared --enable-static && "
         f"make -j$(nproc) install"),

        ("BUILD_LIBTIRPC",
         f"cd {PROJECT_ROOT}/oss_sovereignty/libtirpc-1.3.4 && "
         f"./configure --prefix={STAGING} --disable-gssapi --enable-static --disable-shared && "
         f"make -j$(nproc) install"),

        ("PATCH_ZFS", f"cd {ZSRC} && sed -i 's/highbit64/zstream_highbit64/g' cmd/zstream/zstream_redup.c"),

        ("BUILD_ZFS_USERLAND", 
         f"cd {ZSRC} && "
         f"make distclean || true && "
         f"export PKG_CONFIG_PATH={STAGING}/lib/pkgconfig && "
         f"export LDFLAGS='-L{STAGING}/lib' && "
         f"export CPPFLAGS='-I{STAGING}/include -I{STAGING}/include/tirpc' && "
         f"./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-static --disable-shared --with-config=user --with-tirpc && "
         f"make -j$(nproc)"),

        ("PREP_KERNEL",
         f"cd {KSRC} && "
         f"make defconfig && "
         f"sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && "
         f"sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && "
         f"make olddefconfig && "
         f"make -j$(nproc) modules_prepare && make -j$(nproc) scripts"),

        ("BUILD_ZFS_MODULES",
         f"cd {ZSRC} && "
         f"./configure --with-linux={KSRC} --with-linux-obj={KSRC} --with-config=kernel && "
         f"make -C module -j$(nproc)"),

        ("BUILD_KERNEL_IMAGE",
         f"cd {KSRC} && "
         f"make -j$(nproc) bzImage && "
         f"cp arch/x86/boot/bzImage {PROJECT_ROOT}/build_artifacts/"),

        ("PREP_ROOTFS",
         f"rm -rf {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"mkdir -p {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"cd {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"mkdir -p bin sbin etc proc sys dev usr/bin usr/sbin lib/modules"),

        ("INSTALL_BUSYBOX",
         f"cd {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1 && "
         f"make install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),

        ("INSTALL_ZFS_TOOLS",
         f"cd {ZSRC} && "
         f"make install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),

        ("INSTALL_ZFS_MODULES",
         f"cd {ZSRC} && "
         f"make -C module install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),

        ("WRITE_INIT",
         f"cat <<EOF > {PROJECT_ROOT}/build_artifacts/rootfs_stage/init\n"
         f"#!/bin/sh\n"
         f"mount -t proc proc /proc\n"
         f"mount -t sysfs sys /sys\n"
         f"mount -t devtmpfs dev /dev\n"
         f"modprobe zfs\n"
         f"echo 'AnvilOS Alpha (ZFS) starting...'\n"
         f"exec /bin/sh\n"
         f"EOF\n"
         f"chmod +x {PROJECT_ROOT}/build_artifacts/rootfs_stage/init"),

        ("PACK_INITRAMFS",
         f"cd {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"find . | cpio -o -H newc | gzip > {PROJECT_ROOT}/build_artifacts/initramfs.cpio.gz")
    ]

    for i, (name, cmd) in enumerate(cards):
        uid = str(uuid.uuid4())
        pld = json.dumps({"cmd": cmd})
        cur.execute("INSERT INTO card_stack (id, seq, op, pld, stat) VALUES (?, ?, ?, ?, ?)",
                    (uid, i, "SYS_CMD", pld, 0))
    
    conn.commit()
    conn.close()
    print(f"[*] Injecting {len(cards)} Phase 2 (FIXED V9) Cards...")

if __name__ == "__main__":
    inject()
