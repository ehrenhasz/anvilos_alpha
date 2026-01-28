import sqlite3
import os
import json
import uuid
DB_PATH = "data/cortex.db"
PROJECT_ROOT = os.getcwd()
STAGING = f"{PROJECT_ROOT}/oss_sovereignty/staging"
KSRC = f"{PROJECT_ROOT}/oss_sovereignty/linux-6.6.14"
ZSRC = f"{PROJECT_ROOT}/oss_sovereignty/zfs-2.2.2"
BSRC = f"{PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1"
ARTIFACTS = f"{PROJECT_ROOT}/build_artifacts"
ROOTFS_STAGE = f"{ARTIFACTS}/rootfs_stage"
ISO_STAGE = f"{ARTIFACTS}/iso"
def inject():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("DELETE FROM card_stack")
    cards = [
        ("1. SETUP", "CLEAN_ENV", 
         f"rm -rf {STAGING} {ROOTFS_STAGE} {ISO_STAGE} && "
         f"mkdir -p {STAGING}/lib {STAGING}/include {ROOTFS_STAGE} {ISO_STAGE}/boot"),
        ("2. BUILD", "BUILD_ZLIB_STATIC", 
         f"cd {PROJECT_ROOT}/oss_sovereignty/zlib-1.3.1 && "
         f"./configure --prefix={STAGING} --static && "
         f"make -j$(nproc) install"),
        ("2. BUILD", "BUILD_LIBUUID_BLKID_STATIC",
         f"cd {PROJECT_ROOT}/oss_sovereignty/util-linux-2.39.3 && "
         f"./configure --prefix={STAGING} --disable-all-programs --enable-libuuid --enable-libblkid --disable-bash-completion --disable-shared --enable-static && "
         f"make -j$(nproc) install"),
        ("2. BUILD", "BUILD_LIBTIRPC_STATIC",
         f"cd {PROJECT_ROOT}/oss_sovereignty/libtirpc-1.3.4 && "
         f"./configure --prefix={STAGING} --disable-gssapi --enable-static --disable-shared && "
         f"make -j$(nproc) install"),
        ("2. BUILD", "CONFIG_BUSYBOX",
         f"cd {BSRC} && make defconfig && "
         f"sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config && "
         f"sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config"), # Fix for TC build failure
        ("2. BUILD", "BUILD_BUSYBOX",
         f"cd {BSRC} && make -j$(nproc)"),
        ("2. BUILD", "INSTALL_BUSYBOX",
         f"cd {BSRC} && make install DESTDIR={ROOTFS_STAGE}"),
        ("2. BUILD", "PATCH_ZFS_CODE", 
         f"cd {ZSRC} && sed -i 's/highbit64/zstream_highbit64/g' cmd/zstream/zstream_redup.c"),
        ("2. BUILD", "BUILD_ZFS_USERLAND", 
         f"cd {ZSRC} && "
         f"make distclean || true && "
         f"export PKG_CONFIG_PATH={STAGING}/lib/pkgconfig && "
         f"export LDFLAGS='-L{STAGING}/lib' && "
         f"export CPPFLAGS='-I{STAGING}/include -I{STAGING}/include/tirpc' && "
         f"./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-static --disable-shared --with-config=user --with-tirpc && "
         f"make -j$(nproc)"),
        ("2. BUILD", "INSTALL_ZFS_TOOLS",
         f"cd {ZSRC} && "
         f"make install DESTDIR={ROOTFS_STAGE}"),
        ("3. KERNEL", "PREP_KERNEL_CONFIG",
         f"cd {KSRC} && "
         f"make defconfig && "
         f"sed -i 's/# CONFIG_BLK_DEV_LOOP is not set/CONFIG_BLK_DEV_LOOP=y/' .config && "
         f"sed -i 's/# CONFIG_BLK_DEV_INITRD is not set/CONFIG_BLK_DEV_INITRD=y/' .config && "
         f"make olddefconfig && "
         f"make -j$(nproc) modules_prepare && make -j$(nproc) scripts"),
        ("3. KERNEL", "BUILD_ZFS_MODULES",
         f"cd {ZSRC} && "
         f"./configure --with-linux={KSRC} --with-linux-obj={KSRC} --with-config=kernel && "
         f"make -C module -j$(nproc)"),
        ("3. KERNEL", "INSTALL_ZFS_MODULES_TO_ROOTFS",
         f"cd {ZSRC} && "
         f"make -C module install DESTDIR={ROOTFS_STAGE}"),
        ("3. KERNEL", "BUILD_KERNEL_BZIMAGE",
         f"cd {KSRC} && "
         f"make -j$(nproc) bzImage && "
         f"cp arch/x86/boot/bzImage {ISO_STAGE}/boot/vmlinuz"),
        ("4. ROOTFS", "CREATE_SPARSE_ROOTFS_IMG",
         f"dd if=/dev/zero of={ARTIFACTS}/rootfs.img bs=1M count=500"),
        ("4. ROOTFS", "FORMAT_ROOTFS_POOL",
         f"sudo zpool create -f -o ashift=12 -O compression=lz4 -O mountpoint=none -R {ARTIFACTS}/mnt_pool anvilpool {ARTIFACTS}/rootfs.img && "
         f"sudo zfs create -o mountpoint=/ anvilpool/root && "
         f"sudo zpool export anvilpool"),
        ("4. ROOTFS", "POPULATE_ROOTFS_POOL",
         f"mkdir -p {ARTIFACTS}/mnt_pool && "
         f"sudo zpool import -R {ARTIFACTS}/mnt_pool -d {ARTIFACTS} anvilpool && "
         f"sudo rsync -av {ROOTFS_STAGE}/ {ARTIFACTS}/mnt_pool/ && "
         f"sudo zpool export anvilpool"),
        ("5. INIT", "WRITE_INIT_SCRIPT",
         f"cat <<EOF > {ROOTFS_STAGE}/init\n"
         f"#!/bin/sh\n"
         f"mount -t proc proc /proc\n"
         f"mount -t sysfs sys /sys\n"
         f"mount -t devtmpfs dev /dev\n"
         f"echo 'Loading ZFS Modules...'\n"
         f"modprobe zfs\n"
         f"echo 'Importing Anvil Pool...'\n"
         f"echo 'Welcome to AnvilOS (Phase 2)'\n"
         f"exec /bin/sh\n"
         f"EOF\n"
         f"chmod +x {ROOTFS_STAGE}/init"),
        ("5. INIT", "PACK_INITRAMFS",
         f"cd {ROOTFS_STAGE} && "
         f"find . | cpio -o -H newc | gzip > {ISO_STAGE}/boot/initramfs.cpio.gz"),
        ("6. ISO", "GENERATE_ISOLINUX_CFG",
         f"mkdir -p {ISO_STAGE}/isolinux && "
         f"cat <<EOF > {ISO_STAGE}/isolinux/isolinux.cfg\n"
         f"DEFAULT anvil\n"
         f"LABEL anvil\n"
         f"  KERNEL /boot/vmlinuz\n"
         f"  INITRD /boot/initramfs.cpio.gz\n"
         f"  APPEND console=ttyS0 quiet\n"
         f"EOF"),
        ("6. ISO", "BURN_ISO",
         f"grub-mkrescue -o {PROJECT_ROOT}/anvilos-phase2.iso {ISO_STAGE} || "
         f"xorriso -as mkisofs -o {PROJECT_ROOT}/anvilos-phase2.iso -R -J {ISO_STAGE}")
    ]
    for i, (cat, name, cmd) in enumerate(cards):
        uid = str(uuid.uuid4())
        pld = json.dumps({"cmd": cmd, "desc": f"{cat}: {name}", "_source": "COMMANDER"})
        cur.execute("INSERT INTO card_stack (id, seq, op, pld, stat) VALUES (?, ?, ?, ?, ?)",
                    (uid, i, "SYS_CMD", pld, 0))
    conn.commit()
    conn.close()
    print(f"[*] Injected {len(cards)} Phase 2 (FULL STACK) Cards into Cortex.")
if __name__ == "__main__":
    inject()
