# PHASE 2: THE INJECTION - EXECUTION DIRECTIVES (ZFS MONOLITH)

> **MANDATE:** WE ONLY CODE IN ANVIL. ALL OTHERS ARE BANNED.
> **GUIDE:** Adhere strictly to `DOCS/ANVIL_CODING_GUIDE.md`.
> **PRIME DIRECTIVE:** Use `architect_daemon.py` to create these cards.

## 0. CODING STANDARDS (The Law)
- [ ] **MicroJSON ONLY**: ALL JSON payloads, logs, and persistence MUST use MicroJSON (RFC-0002).
- [ ] **100-Line Limit**: Atomic units only. No file > 100 lines.
- [ ] **Forbidden**: `requests`, `subprocess`, `threading`, and raw `sqlite3` in runtime.

## 1. SOURCE PREPARATION (The Foundation)
- [x] **Card 100:** `fetch_zfs_source` (Download & Extract OpenZFS 2.2.2)
- [ ] **Card 101:** `clean_linux_source` (Run `make mrproper` in `oss_sovereignty/linux-6.6.14`)
- [ ] **Card 102:** `clean_busybox_source` (Run `make mrproper` in `oss_sovereignty/busybox-1.36.1`)

## 2. USERLAND FORGING (The Muscles)
### Busybox (Static)
- [ ] **Card 200:** `config_busybox_static` (Generate `.config` with `CONFIG_STATIC=y`)
- [ ] **Card 201:** `build_busybox` (Run `make -j$(nproc)`)
- [ ] **Card 202:** `install_busybox` (Install to `build_artifacts/rootfs_stage/bin`)

### ZFS Utils (Static)
- [ ] **Card 210:** `prep_zfs_build` (Install deps: `zlib-devel`, `libuuid-devel`, `libblkid-devel` if needed inside container)
- [ ] **Card 211:** `config_zfs_static` (Run `./configure --enable-static --disable-shared --with-config=user`)
- [ ] **Card 212:** `build_zfs_utils` (Run `make -j$(nproc)`)
- [ ] **Card 213:** `install_zfs_utils` (Copy `zpool`, `zfs` to `build_artifacts/rootfs_stage/sbin`)

## 3. KERNEL FORGING (The Heart)
- [ ] **Card 300:** `prepare_kernel_headers` (Install headers for ZFS build compatibility)
- [ ] **Card 301:** `graft_zfs_modules` (Configure ZFS as kernel module source or built-in if patched)
    * *Note: ZFS usually builds as modules. For Monolith, we might need to link statically or use initramfs to load modules.*
    * *Refined Strategy: Build ZFS modules against kernel, then install modules to rootfs.*
- [ ] **Card 302:** `config_kernel_monolith` (Enable `CONFIG_BLK_DEV_LOOP`, `CONFIG_BLK_DEV_INITRD`, `CONFIG_MODULES`)
- [ ] **Card 303:** `build_kernel_bzImage` (Run `make bzImage`)
- [ ] **Card 304:** `build_kernel_modules` (Run `make modules`)
- [ ] **Card 305:** `install_kernel_artifacts` (Copy `bzImage` to `build_artifacts/iso/boot`)

## 4. THE ZFS ROOT (The Body)
- [ ] **Card 400:** `create_sparse_rootfs` (Create 500MB `rootfs.img`)
- [ ] **Card 401:** `format_rootfs_pool` (Loopback mount -> `zpool create` -> Export)
    * *Constraint: This requires privileged access. If container fails, do this on host manually.*
- [ ] **Card 402:** `mount_and_populate` (Import pool, Copy `rootfs_stage/*` to pool, Export)

## 5. THE INITRAMFS (The Key)
- [ ] **Card 500:** `write_init_script` (Python/Shell script to: Mount ISO -> Loop `rootfs.img` -> Import Pool -> Switch Root)
- [ ] **Card 501:** `pack_initramfs` (CPIO + Gzip `build_artifacts/initramfs_stage`)

## 6. FINAL ARTIFACT (The Anvil)
- [ ] **Card 600:** `generate_isolinux_cfg` (Write `isolinux.cfg` boot menu)
- [ ] **Card 601:** `burn_iso` (Run `xorriso` to create `anvilos-phase2.iso`)
