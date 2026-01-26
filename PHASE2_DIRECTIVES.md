# PHASE 2: THE INJECTION - EXECUTION DIRECTIVES (ZFS MONOLITH)

> **MANDATE:** WE ONLY CODE IN ANVIL. ALL OTHERS ARE BANNED.
> **GUIDE:** Adhere strictly to `DOCS/ANVIL_CODING_GUIDE.md`.
> **PRIME DIRECTIVE:** If it runs on python3, it is not Anvil code. It is a prototype. If it runs on `/usr/local/bin/anvil`, it is Law.

## 0. CODING STANDARDS (The Law)
- [ ] **Hermetic Toolchain**: Production code must be forged for the Anvil MicroPython runtime.
- [ ] **MicroJSON ONLY**: ALL JSON payloads, logs, and persistence MUST use MicroJSON (RFC-0002). Standard JSON is BANNED.
- [ ] **100-Line Limit**: Atomic units only. No file > 100 lines.
- [ ] **Forbidden**: `requests`, `subprocess`, `threading`, and raw `sqlite3` are BANNED in the runtime.

## 1. SOURCE ACQUISITION (The Materials)
- [ ] **OpenZFS**: Fetch OpenZFS v2.2.2 source tarball to `oss_sovereignty/zfs`.
- [ ] **Clean Source**: Ensure `oss_sovereignty/linux-6.6.14` and `busybox` are clean.

## 2. USERLAND PREPARATION (The Muscles)
- [ ] **Busybox**: Config `CONFIG_STATIC=y`. Build `busybox`.
- [ ] **ZFS Utils**: Build static `zpool` and `zfs` binaries (requires hacking Makefiles or using `musl-gcc` wrappers).
- [ ] **Artifacts**: Place `busybox`, `zpool`, `zfs` in `build_artifacts/bin`.

## 3. KERNEL FORGING (The Heart)
- [ ] **Preparation**: Copy OpenZFS source into `linux-6.6.14/drivers/zfs` (or use `--add-drivers` patch).
- [ ] **Configuration**:
    -   `CONFIG_ZFS=y` (Built-in, NOT module).
    -   Disable Ext4, Btrfs, XFS (`# CONFIG_EXT4_FS is not set`).
    -   `CONFIG_BLK_DEV_LOOP=y` (Essential for mounting rootfs.img).
    -   `CONFIG_BLK_DEV_INITRD=y`.
- [ ] **Compilation**: `make bzImage`.
- [ ] **Artifact**: `build_artifacts/bzImage`.

## 4. THE ZFS ROOT (The Body)
- [ ] **Loopback Creation**: Create 500MB file `build_artifacts/rootfs.img`.
- [ ] **Pool Creation**:
    -   `zpool create -o ashift=12 -O compression=lz4 -O mountpoint=none -R /mnt/anvil_temp anvil_pool /path/to/rootfs.img`.
    -   `zfs create -o mountpoint=/ anvil_pool/ROOT`.
- [ ] **Population**:
    -   Install `busybox` to `/mnt/anvil_temp/bin/busybox`.
    -   Install `zfs`, `zpool` to `/mnt/anvil_temp/sbin/`.
    -   Create `/init` (ZFS-aware PID 1) in `/mnt/anvil_temp/`.
- [ ] **Export**: `zpool export anvil_pool`.

## 5. THE INITRAMFS (The Key)
- [ ] **Script**: Create `init` script that:
    1.  Mounts ISO/CDROM.
    2.  `losetup /dev/loop0 /cdrom/rootfs.img`.
    3.  `zpool import -d /dev/loop0 -f anvil_pool`.
    4.  `mount -t zfs anvil_pool/ROOT /new_root`.
    5.  `switch_root /new_root /init`.
- [ ] **Pack**: `find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz`.

## 6. FINAL ARTIFACT (The Anvil)
- [ ] **ISO Structure**:
    -   `/boot/bzImage`
    -   `/boot/initramfs.cpio.gz`
    -   `/rootfs.img` (The ZFS Pool)
    -   `/isolinux/isolinux.cfg`
- [ ] **Burn**: `xorriso` to generate `anvilos-alpha.iso`.
