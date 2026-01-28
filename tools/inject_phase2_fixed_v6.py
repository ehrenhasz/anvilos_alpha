import sqlite3
import os
import json
import uuid
DB_PATH = "data/cortex.db"
PROJECT_ROOT = os.getcwd()
def inject():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("DELETE FROM card_stack")
    cards = [
        ("PATCH_ZFS_ZSTREAM", f"cd {PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2 && sed -i 's/highbit64/zstream_highbit64/g' cmd/zstream/zstream_redup.c"),
        ("BUILD_ZFS_USERLAND", 
         f"cd {PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2 && "
         f"./autogen.sh && "
         f"LDFLAGS='-L{PROJECT_ROOT}/oss_sovereignty/staging/lib' "
         f"CPPFLAGS='-I{PROJECT_ROOT}/oss_sovereignty/staging/include' "
         f"./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-linux-builtin=yes --with-linux={PROJECT_ROOT}/oss_sovereignty/linux-6.6.14 --enable-static --disable-shared && "
         f"make -j$(nproc)"),
        ("PREP_KERNEL",
         f"cd {PROJECT_ROOT}/oss_sovereignty/linux-6.6.14 && "
         f"make defconfig && "
         f"sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && "
         f"sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && "
         f"make olddefconfig && "
         f"make prepare && make scripts && make modules_prepare"),
        ("BUILD_ZFS_MODULES",
         f"cd {PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2 && "
         f"./configure --with-linux={PROJECT_ROOT}/oss_sovereignty/linux-6.6.14 --with-linux-obj={PROJECT_ROOT}/oss_sovereignty/linux-6.6.14 && "
         f"make -C module -j$(nproc)"),
        ("BUILD_KERNEL_IMAGE",
         f"cd {PROJECT_ROOT}/oss_sovereignty/linux-6.6.14 && "
         f"make -j$(nproc) bzImage && "
         f"cp arch/x86/boot/bzImage {PROJECT_ROOT}/build_artifacts/ப்புகளை"),
        ("PREP_ROOTFS",
         f"mkdir -p {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"cd {PROJECT_ROOT}/build_artifacts/rootfs_stage && "
         f"mkdir -p bin sbin etc proc sys dev usr/bin usr/sbin lib/modules"),
        ("INSTALL_BUSYBOX",
         f"cd {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1 && "
         f"make install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),
        ("INSTALL_ZFS_TOOLS",
         f"cd {PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2 && "
         f"make install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),
        ("INSTALL_ZFS_MODULES",
         f"cd {PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2 && "
         f"make -C module install DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage"),
        ("WRITE_INIT",
         f"cat <<EOF > {PROJECT_ROOT}/build_artifacts/rootfs_stage/init\n"
         "#!/bin/sh\n"
         "mount -t proc proc /proc\n"
         "mount -t sysfs sys /sys\n"
         "mount -t devtmpfs dev /dev\n"
         "modprobe zfs\n"
         "echo 'AnvilOS Alpha (ZFS) starting...\n'"
         "exec /bin/sh\n"
         "EOF\n"
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
    print(f"[*] Injecting {len(cards)} Phase 2 (FIXED V6) Cards...")
if __name__ == "__main__":
    inject()
