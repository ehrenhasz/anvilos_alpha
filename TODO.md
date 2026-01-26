# PROJECT ANVIL: ATOMIC TACTICAL BACKLOG

> **PRIME DIRECTIVE (UPDATED - JAN 23 2026):** 
> 1. **CONTAINERIZATION:** All compilation and execution logic runs in `anvil-gentoo-container`.
> 2. **NO LOCAL STATE:** The container uses the HOST's `cortex.db` via bind mount. No local DBs.
> 3. **SOURCE CONTROL:** NO direct source code in the container. ONLY `recipe.py` (Cards) and `microjson` are allowed.
> 4. **OUTPUT CONTROL:** The ONLY permitted outputs are Anvil Code (Binaries) and Human JSON. Everything else is BANNED.
> 5. **LOOP:** The Forgemaster (Coder) writes recipes from our `ROADMAP.md` -> Mainframe processes -> Anvil Code/JSON output. Loop until stuck.
> 6. **INTERVENTION:** If stuck, report error to Gemini CLI. We fix. Otherwise, HANDS OFF.

## PHASE 1: THE MAINFRAME (ORCHESTRATION) [P1]
### 1.1 MicroJSON Protocol (Simulation Spec)
- [x] Create `runtime/microjson/__init__.py` module
- [x] Implement `MicroJSON.encode(payload)` in `runtime/microjson/codec.py`
- [x] Implement `MicroJSON.decode(payload)` in `runtime/microjson/codec.py`
- [x] Add unit test `tests/test_microjson.py` for encoding
- [x] Add unit test `tests/test_microjson.py` for decoding

### 1.2 Cortex Database (Schema Update)
- [x] Create `runtime/cortex/db_interface.py`
- [x] Implement `CortexDB.__init__(db_path)`
- [x] Implement `CortexDB.push_card` with `ret` and `timestamp` fields
- [x] Implement `CortexDB.pop_card`
- [x] Implement `CortexDB.peek_stack`
- [x] Implement `CortexDB.log_result`

### 1.3 Architect Daemon (The Writer)
- [x] Create `runtime/architect_daemon.py`
- [x] Integrate `MicroJSON` packing
- [x] Implement logic to clear deck (`clear_deck`)
- [x] Implement Recipe Generation (GEN_VAR, GEN_IO, GEN_LOG...)
- [x] Connect to `CortexDB`

### 1.4 Processor Daemon (The Reader)
- [x] Create `runtime/processor_daemon.py`
- [x] Implement Polling Loop (Status 0 -> 1)
- [x] Implement `AISimulation` (Mock Endpoint)
- [x] Implement Status Stamping (Success=2, Error=9)

## PHASE 4: THE MONITOR (OBSERVABILITY) [P1]
### 4.1 UI Components (Rich Library)
- [x] Create `runtime/mainframe_monitor.py`
- [x] Implement `MainframeMonitor` class
- [x] Implement `fetch_cards()` (Last 12)
- [x] Implement `fetch_stats()` (Group by Status)
- [x] Implement `get_status_text(code)` helper

### 4.2 Dashboard Layout
- [x] Implement `generate_header()` (Project Name, Time)
- [x] Implement `generate_stack_view()` (Table: SEQ, ID, OP, PLD, STATUS)
- [x] Implement `generate_telemetry()` (Stats Panel, System Status)
- [x] Implement `generate_footer()` (Commands)

### 4.3 Runtime
- [x] Implement `run()` loop with `Live` context manager
- [x] Set refresh rate to 4Hz

## PHASE 1.5: THE SUPPLY CHAIN (GOVERNANCE)
### 1.5.1 ASCII_OSS Hierarchy
- [x] Create directory `/mnt/anvil_temp/ascii_oss`
- [x] Create directory `/mnt/anvil_temp/ascii_oss/src`
- [x] Create directory `/mnt/anvil_temp/ascii_oss/bin`
- [x] Create directory `/mnt/anvil_temp/ascii_oss/lib`
- [x] Create directory `/mnt/anvil_temp/ascii_oss/etc`
- [x] Create `README.md` in `ascii_oss/` explaining provenance

### 1.5.2 The Spy (Audit)
- [x] Create `runtime/agents/spy.py`
- [x] Implement `Spy.watch_path(path)` using `inotify`
- [x] Implement `Spy.log_event(event)` to `cortex.db`
- [x] Implement `Spy.audit_db_transaction(tx_id)`
- [x] Create systemd service `system/services/anvil-spy.service`

### 1.5.3 The Witch (Ratification)
- [x] Create `runtime/agents/witch.py`
- [x] Implement `Witch.fetch_pending_rfcs()`
- [x] Implement `Witch.sign_artifact(artifact_hash)`
- [x] Implement `Witch.reject_artifact(artifact_hash)`

## PHASE 2A: THE MONOLITH (INFRASTRUCTURE) [P0]
> Ref: PHASE2_DIRECTIVES.md

### 2A.1 Source Acquisition
- [x] **OpenZFS**: Fetch OpenZFS v2.2.2 source tarball to `oss_sovereignty/zfs`.
- [ ] **Clean Source**: Ensure `oss_sovereignty/linux-6.6.14` and `busybox` are clean.

### 2A.2 Userland Preparation (The Muscles)
- [ ] **Busybox**: Config `CONFIG_STATIC=y`. Build `busybox`.
- [ ] **ZFS Utils**: Build static `zpool` and `zfs` binaries.
- [ ] **Artifacts**: Place `busybox`, `zpool`, `zfs` in `build_artifacts/bin`.

### 2A.3 Kernel Forging (The Heart)
- [ ] **Preparation**: Copy OpenZFS source into `linux-6.6.14/drivers/zfs`.
- [ ] **Configuration**: Monolithic, `CONFIG_ZFS=y`, `CONFIG_BLK_DEV_LOOP=y`, `CONFIG_BLK_DEV_INITRD=y`.
- [ ] **Compilation**: `make bzImage`.
- [ ] **Artifact**: `build_artifacts/bzImage`.

### 2A.4 The ZFS Root (The Body)
- [ ] **Loopback**: Create 500MB `rootfs.img`.
- [ ] **Pool**: Create `anvil_pool` and `anvil_pool/ROOT`.
- [ ] **Population**: Install busybox, zfs, zpool, and custom `/init`.
- [ ] **Export**: `zpool export anvil_pool`.

### 2A.5 The Initramfs (The Key)
- [ ] **Script**: Create `init` script (Mount ISO -> Loop -> Import Pool -> Switch Root).
- [ ] **Pack**: Generate `initramfs.cpio.gz`.

### 2A.6 Final Artifact
- [ ] **ISO**: Generate `anvilos-phase2.iso` with kernel, initramfs, and rootfs.img.

## PHASE 2B: THE INJECTION (PAYLOAD)
### 2.1 ANVIL-CC (Sovereign Compiler)
- [x] Inject `sys_09_Anvil` source
- [x] Configure cross-toolchain symlinks (`ext/toolchain`)
- [x] Build `mpy-cross` host tool
- [x] Compile static `anvil` binary (Frozen `anvil.py`)
- [x] Verify static linkage and strip binary
- [x] Install `anvil` to `ascii_oss/bin`

### 2.2 ANVIL-VMM (Hypervisor)
- [ ] Clone `firecracker` repo to `ascii_oss/src`
- [ ] Remove `metrics` crate dependency in `Cargo.toml`
- [ ] Remove `logger` crate dependency in `Cargo.toml`
- [ ] Create `vmm_config.json` for static musl build
- [ ] Run `cargo build --target x86_64-unknown-linux-musl --release`
- [ ] Strip debug symbols from binary
- [ ] Verify `anvil-vmm` static linkage (`ldd`)

### 2.3 ANVIL-DB (Database)
- [ ] Fetch `postgresql-16` source to `ascii_oss/src`
- [ ] Configure build with `--without-readline`
- [ ] Configure build with `--without-zlib`
- [ ] Configure build with `--without-perl`
- [ ] Configure build with `--without-python`
- [ ] Compile `postgres` binary
- [ ] Verify `postgres` binary static linkage

### 2.4 The Jail Cell
- [ ] Create `runtime/jail/init.c` (PID 1 logic)
- [ ] Compile `init` static binary
- [ ] Create `rootfs` directory structure
- [ ] Copy `init` to `rootfs/sbin/init`

## PHASE 3: THE TESSERACT (CRYPTO)
### 3.1 Warp Core
- [ ] Create `runtime/tesseract/warp_core.py`
- [ ] Implement `WarpCore.trigger_sigfpe()`
- [ ] Implement `WarpCore.catch_sigfpe()`
- [ ] Implement `WarpCore.read_rdtsc()`
- [ ] Implement `WarpCore.shake256_whitening(raw_entropy)`

### 3.2 Dilithium Crystal
- [ ] Create `runtime/tesseract/dilithium.py`
- [ ] Import `liboqs` wrapper
- [ ] Implement `Dilithium.generate_keypair()`
- [ ] Implement `Dilithium.sign(message)`
- [ ] Implement `Dilithium.verify(message, signature)`

### 3.3 The Engine
- [ ] Create `runtime/tesseract/engine.py`
- [ ] Implement `Tesseract.init_grid(3x3)`
- [ ] Implement `Tesseract.rotate_4d(time_t)`
- [ ] Implement `Tesseract.get_shard(x, y)`

### 3.4 Quantum Mine
- [ ] Create SQL migration `db/migrations/001_quantum_mine.sql`
- [ ] Define table `raw_ore`
- [ ] Implement `Synapse.log_opcode(opcode, entropy_snapshot)`

## PHASE 5: THE CRYSTAL PALACE (USERLAND)
### 5.1 Protocol Teleportation
- [ ] Create `runtime/ipc/shm_manager.py`
- [ ] Implement `ShmManager.create_segment(name, size)`
- [ ] Implement `ShmManager.write_segment(name, data)`
- [ ] Implement `ShmManager.read_segment(name)`
- [ ] Implement `ShmManager.destroy_segment(name)`

### 5.2 The Collar
- [ ] Create `runtime/security/collar.py`
- [ ] Implement `Collar.sanitize_input(input_str)`
- [ ] Implement `Collar.check_entropy_request(source)`
- [ ] Implement `Collar.block_unsanctioned_commands(cmd)`

### 5.3 The Warden
- [ ] Create `runtime/security/warden.py`
- [ ] Implement `Warden.monitor_stdout(pid)`
- [ ] Implement `Warden.detect_hallucination(text)`
- [ ] Implement `Warden.issue_sigstop(pid)`
- [ ] Implement `Warden.rollback_container(container_id)`

### 5.4 Anvil Greeter
- [ ] Create `runtime/userland/greeter.py` (TUI)
- [ ] Implement `Greeter.prompt_password()`
- [ ] Implement `Greeter.hash_password(argon2)`
- [ ] Implement `Greeter.prompt_2fa(nonce)`
- [ ] Implement `Greeter.verify_2fa(signature)`

## PHASE 6: DOCUMENTATION (RFC-059) [P2]
- [ ] **[MAN]** Create man pages for Anvil Coding Standards.
- [ ] **[MAN]** Create man pages for MicroJSON & Mainframe.
- [ ] **[MAN]** Create man pages for The Committee.

## PHASE 7: THE FOUNDATION (LEGACY/BACKLOG)
### 7.1 Preparation
- [ ] Boot Live CD
- [ ] Verify `ping 8.8.8.8`
- [ ] Run `fdisk /dev/sda` (Wipe)
- [ ] Create partition `/dev/sda1` (Boot)
- [ ] Create partition `/dev/sda2` (Root)
- [ ] Format `/dev/sda1` as vfat
- [ ] Format `/dev/sda2` as ext4
- [ ] Mount `/dev/sda2` to `/mnt/gentoo`

### 7.2 Stage3
- [ ] Download Stage3 tarball
- [ ] Verify GPG signature of Stage3
- [ ] Untar Stage3 to `/mnt/gentoo`
- [ ] Verify `xattrs` are preserved

## PHASE 8: THE PROFILE
### 8.1 Chroot Setup
- [ ] Copy `/etc/resolv.conf` to `/mnt/gentoo/etc/`
- [ ] Mount `/proc`
- [ ] Mount `/sys`
- [ ] Mount `/dev`
- [ ] Chroot into `/mnt/gentoo`

### 8.2 Profile Selection
- [ ] Run `emerge-webrsync`
- [ ] Run `eselect profile list`
- [ ] Run `eselect profile set default/linux/amd64/23.0/no-multilib/hardened/selinux`

## PHASE 9: YOUR KERNEL
### 9.1 Sources
- [ ] Run `emerge sys-kernel/gentoo-sources`
- [ ] Symlink `/usr/src/linux`

### 9.2 Configuration
- [ ] Run `make menuconfig`
- [ ] Enable `CONFIG_SECURITY_SELINUX`
- [ ] Enable `CONFIG_AUDIT`
- [ ] Enable `CONFIG_PAGE_POISONING`
- [ ] Enable `CONFIG_SLAB_FREELIST_HARDENED`
- [ ] Save `.config`

### 9.3 Compilation
- [ ] Run `make -j$(nproc)`
- [ ] Run `make modules_install`
- [ ] Copy `arch/x86/boot/bzImage` to `/boot/vmlinuz-anvil`

## PHASE 10: THE USERSPACE
### 10.1 Core Utils
- [ ] Emerge `sysklogd`
- [ ] Emerge `cronie`
- [ ] Emerge `dhcpcd`
- [ ] Emerge `policycoreutils`
- [ ] Emerge `selinux-base-policy`

## PHASE 11: THE LABELING
### 11.1 Config
- [ ] Edit `/etc/selinux/config`
- [ ] Set `SELINUX=permissive`
- [ ] Set `SELINUXTYPE=mcs`

### 11.2 Application
- [ ] Run `rlpkg -a -r` (Relabel All)

## PHASE 12: THE BOOTLOADER
### 12.1 GRUB
- [ ] Emerge `sys-boot/grub`
- [ ] Install GRUB to `/dev/sda`
- [ ] Edit `/etc/default/grub`
- [ ] Add kernel params: `security=selinux selinux=1`
- [ ] Run `grub-mkconfig -o /boot/grub/grub.cfg`

## PHASE 13: THE FINAL HANDOVER
### 13.1 Lockdown
- [ ] Edit `/etc/selinux/config`
- [ ] Set `SELINUX=enforcing`
- [ ] Reboot system

## PHASE 14: THE FINAL FORM (ISO)
### 14.1 Build Script
- [ ] Create `scripts/mkiso.sh`
- [ ] Add logic to create `isolinux` directory
- [ ] Add logic to copy kernel and initramfs
- [ ] Add `xorriso` command generation

### 14.2 Installer
- [ ] Create `scripts/anvil-install.sh`
- [ ] Add disk detection logic
- [ ] Add partition logic
- [ ] Add mode selection (Dev/Server)

### 14.3 Verification
- [ ] Validate ISO checksum
- [ ] Boot ISO in QEMU