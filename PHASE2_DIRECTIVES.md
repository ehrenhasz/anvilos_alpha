# PHASE 2: THE INJECTION - EXECUTION DIRECTIVES (ZFS MONOLITH)

> **MANDATE:** ANVIL IS LAW. SOVEREIGN TOOLCHAIN ONLY.
> **CONSTRAINT:** NO HOST TOOLS (`/usr/bin/cc` BANNED).
> **GRANULARITY:** ATOMIC OPERATIONS ONLY.

## 0. PRE-FLIGHT (The Collar)
- [ ] **Card 001:** `verify_sovereign_toolchain` (Check `ext/toolchain/bin/x86_64-bicameral-linux-musl-gcc` exists)
- [ ] **Card 002:** `verify_build_env` (Ensure we are in `oss_sovereignty` or compatible container)

## 1. BUSYBOX FORGE (The Foundation)
- [ ] **Card 100:** `clean_busybox` (Run `make mrproper`)
- [ ] **Card 101:** `gen_busybox_defconfig` (Run `make defconfig`)
- [ ] **Card 102:** `patch_busybox_static` (Set `CONFIG_STATIC=y`)
- [ ] **Card 103:** `patch_busybox_no_tc` (Disable `CONFIG_TC` to avoid header conflicts)
- [ ] **Card 104:** `build_busybox_bin` (Run `make` with Sovereign GCC)
- [ ] **Card 105:** `verify_busybox_static` (Run `readelf` or `file` to confirm static linkage)
- [ ] **Card 106:** `install_busybox_tree` (Install to `build_artifacts/rootfs_stage`)

## 2. STATIC DEPENDENCIES (The Chain)
*ZFS requires static libraries to link into a monolith tool.*
- [ ] **Card 200:** `fetch_zlib_source` (Version 1.3.1)
- [ ] **Card 201:** `build_zlib_static` (Configure static, build, install to staging)
- [ ] **Card 202:** `fetch_util_linux_source` (For libuuid/libblkid)
- [ ] **Card 203:** `build_libuuid_static` (Configure --enable-static, install to staging)
- [ ] **Card 204:** `build_libblkid_static` (Configure --enable-static, install to staging)

## 3. ZFS USERLAND (The Monolith)
- [ ] **Card 300:** `fetch_zfs_source` (OpenZFS 2.2.2)
- [ ] **Card 301:** `patch_zfs_configure` (Fix paths for static deps)
- [ ] **Card 302:** `configure_zfs_static` (Link against staged zlib/uuid/blkid)
- [ ] **Card 303:** `build_zfs_binaries` (Run `make`)
- [ ] **Card 304:** `verify_zfs_static` (Confirm `zpool` is static and strictly linked)
- [ ] **Card 305:** `install_zfs_tools` (Copy `zpool`, `zfs` to `build_artifacts/rootfs_stage/sbin`)

## 4. KERNEL FORGING (The Heart)
- [ ] **Card 400:** `clean_kernel_tree`
- [ ] **Card 401:** `prep_kernel_config` (Enable Monolith features: Loop, Initrd)
- [ ] **Card 402:** `prep_kernel_headers` (For ZFS module build)
- [ ] **Card 403:** `build_zfs_modules` (Build kmods against this kernel)
- [ ] **Card 404:** `build_kernel_bzImage` (The Kernel Image)
- [ ] **Card 405:** `install_kernel_artifact` (Move bzImage to ISO boot)
- [ ] **Card 406:** `install_kernel_modules` (Move ZFS modules to rootfs `/lib/modules`)

## 5. THE ANVIL INIT (The Law)
*We attempt the Sovereign Init again, but CORRECTLY this time.*
- [ ] **Card 500:** `define_init_logic` (Write MicroJSON spec for Init)
- [ ] **Card 501:** `forge_init_source` (Write `src/native/init.anv`)
- [ ] **Card 502:** `transpile_init` (Convert `.anv` -> `.c` using `anvil.py`)
- [ ] **Card 503:** `compile_init_static` (**USE SOVEREIGN GCC ONLY**)
- [ ] **Card 504:** `verify_init_safety` (Check hash, check static)
- [ ] **Card 505:** `install_init_binary` (Place as `/init` in rootfs)

## 6. FINAL ASSEMBLY (The ISO)
- [ ] **Card 600:** `create_rootfs_image` (Sparse file)
- [ ] **Card 601:** `format_rootfs_zfs` (Zpool create)
- [ ] **Card 602:** `populate_rootfs` (Copy stage to zpool)
- [ ] **Card 603:** `pack_initramfs` (CPIO + Gzip)
- [ ] **Card 604:** `gen_isolinux_cfg` (Boot menu)
- [ ] **Card 605:** `burn_iso` (Xorriso)