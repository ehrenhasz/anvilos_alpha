# Forge Directives - Phase 2 (ZFS Monolith)

PHASE2_DIRECTIVES = [
    # 0. PRE-FLIGHT (The Collar)
    {"seq": 1, "op": "verify_sovereign_toolchain", "pld": {"desc": "Check ext/toolchain/bin/x86_64-bicameral-linux-musl-gcc exists"}},
    {"seq": 2, "op": "verify_build_env", "pld": {"desc": "Ensure we are in oss_sovereignty or compatible container"}},

    # 1. BUSYBOX FORGE (The Foundation)
    {"seq": 100, "op": "clean_busybox", "pld": {"desc": "Run make mrproper"}},
    {"seq": 101, "op": "gen_busybox_defconfig", "pld": {"desc": "Run make defconfig"}},
    {"seq": 102, "op": "patch_busybox_static", "pld": {"desc": "Set CONFIG_STATIC=y"}},
    {"seq": 103, "op": "patch_busybox_no_tc", "pld": {"desc": "Disable CONFIG_TC"}},
    {"seq": 104, "op": "build_busybox_bin", "pld": {"desc": "Run make with Sovereign GCC"}},
    {"seq": 105, "op": "verify_busybox_static", "pld": {"desc": "Run readelf to confirm static linkage"}},
    {"seq": 106, "op": "install_busybox_tree", "pld": {"desc": "Install to build_artifacts/rootfs_stage"}},

    # 2. STATIC DEPENDENCIES (The Chain)
    {"seq": 200, "op": "fetch_zlib_source", "pld": {"desc": "Version 1.3.1"}},
    {"seq": 201, "op": "build_zlib_static", "pld": {"desc": "Configure static, build, install to staging"}},
    {"seq": 202, "op": "fetch_util_linux_source", "pld": {"desc": "For libuuid/libblkid"}},
    {"seq": 203, "op": "build_libuuid_static", "pld": {"desc": "Configure --enable-static, install to staging"}},
    {"seq": 204, "op": "build_libblkid_static", "pld": {"desc": "Configure --enable-static, install to staging"}},

    # 3. ZFS USERLAND (The Monolith)
    {"seq": 300, "op": "fetch_zfs_source", "pld": {"desc": "OpenZFS 2.2.2"}},
    {"seq": 301, "op": "patch_zfs_configure", "pld": {"desc": "Fix paths for static deps"}},
    {"seq": 302, "op": "configure_zfs_static", "pld": {"desc": "Link against staged zlib/uuid/blkid"}},
    {"seq": 303, "op": "build_zfs_binaries", "pld": {"desc": "Run make"}},
    {"seq": 304, "op": "verify_zfs_static", "pld": {"desc": "Confirm zpool is static"}},
    {"seq": 305, "op": "install_zfs_tools", "pld": {"desc": "Copy zpool, zfs to rootfs/sbin"}},

    # 4. KERNEL FORGING (The Heart)
    {"seq": 400, "op": "clean_kernel_tree", "pld": {"desc": "Clean kernel source"}},
    {"seq": 401, "op": "prep_kernel_config", "pld": {"desc": "Enable Monolith features: Loop, Initrd"}},
    {"seq": 402, "op": "prep_kernel_headers", "pld": {"desc": "For ZFS module build"}},
    {"seq": 403, "op": "build_zfs_modules", "pld": {"desc": "Build kmods against this kernel"}},
    {"seq": 404, "op": "build_kernel_bzImage", "pld": {"desc": "The Kernel Image"}},
    {"seq": 405, "op": "install_kernel_artifact", "pld": {"desc": "Move bzImage to ISO boot"}},
    {"seq": 406, "op": "install_kernel_modules", "pld": {"desc": "Move ZFS modules to rootfs /lib/modules"}},

    # 5. THE ANVIL INIT (The Law)
    {"seq": 500, "op": "define_init_logic", "pld": {"desc": "Write MicroJSON spec for Init"}},
    {"seq": 501, "op": "forge_init_source", "pld": {"desc": "Write src/native/init.anv"}},
    {"seq": 502, "op": "transpile_init", "pld": {"desc": "Convert .anv -> .c"}},
    {"seq": 503, "op": "compile_init_static", "pld": {"desc": "USE SOVEREIGN GCC ONLY"}},
    {"seq": 504, "op": "verify_init_safety", "pld": {"desc": "Check hash, check static"}},
    {"seq": 505, "op": "install_init_binary", "pld": {"desc": "Place as /init in rootfs"}},

    # 6. FINAL ASSEMBLY (The ISO)
    {"seq": 600, "op": "create_rootfs_image", "pld": {"desc": "Sparse file"}},
    {"seq": 601, "op": "format_rootfs_zfs", "pld": {"desc": "Zpool create"}},
    {"seq": 602, "op": "populate_rootfs", "pld": {"desc": "Copy stage to zpool"}},
    {"seq": 603, "op": "pack_initramfs", "pld": {"desc": "CPIO + Gzip"}},
    {"seq": 604, "op": "gen_isolinux_cfg", "pld": {"desc": "Boot menu"}},
    {"seq": 605, "op": "burn_iso", "pld": {"desc": "Xorriso"}},
]
