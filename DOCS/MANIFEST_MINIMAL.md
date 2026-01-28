# ANVILOS STRICT MINIMALIST MANIFEST
**RFC-2026-000010**
**CLASSIFICATION:** HARD LAW
**TARGET:** PHASE 2 BUILD
## 1. Core Philosophy
The AnvilOS Minimalist Build is designed for absolute sovereignty and minimal attack surface. It rejects all bloat, legacy support, and non-essential utilities.
## 2. Hardware Constraints
- **Architecture:** `x86_64` ONLY.
- **Drivers:** Explicitly enabled ONLY. No "build everything" modules.
- **Legacy:** Disabled (No ISA, No Floppy, No Parallel Port).
## 3. Userland Manifest
### 3.1 Editor
- **Approved:** `vim` (Tiny/Small featureset).
- **Prohibited:** `nano`, `emacs`, `ed`.
### 3.2 Network
- **Stack:** `iproute2`, `dhcpcd` (or static).
- **Remote Access:** `OpenSSH` (Client & Server).
- **Prohibited:** `NetworkManager`, `wpa_supplicant` (unless wifi explicitly requested later), `curl`, `wget` (unless in build env).
### 3.3 Filesystem
- **Core:** `busybox` (coreutils), `util-linux` (libuuid/libblkid).
- **Storage:** `zfs-utils` (zpool/zfs).
- **Prohibited:** `lvm`, `mdadm`, `btrfs`.
### 3.4 Documentation
- **Allowed:** Man pages for *installed* tools only (`/usr/share/man`).
- **Prohibited:** HTML docs, info pages, `/usr/share/doc`, `/usr/share/gtk-doc`.
### 3.5 Locale
- **Allowed:** `en_US.UTF-8` ONLY.
- **Action:** Delete all other locales from `/usr/share/locale`.
## 4. Build Guidelines
1.  **Strip:** All binaries must be stripped (`strip --strip-all`).
2.  **Static:** Prefer static linking where possible (except ZFS/OpenSSH where dlopen might be needed, but aim for static).
3.  **Cleanup:** No source code or headers left in `/usr/src` or `/usr/include` on the final image.
## 5. Verification
The "Warden" (Processor Daemon) and "Matrix" visualizer will confirm adherence to this manifest during the forge process.
## 6. Source Code Mandates
1.  **NO COMMENTS:** All comments (`#`, `//`, `/* */`) are to be stripped from all project scripts (`.py`, `.sh`) and configuration files before final build.
2.  **NO BLANK LINES:** All empty or whitespace-only lines are to be stripped.
3.  **PURPOSE:** The only "documentation" is the MAN page. The only "code" is the logic. This is "The Hold" in practice: everything non-essential is purged.
## 7. Massive Granularity Protocol
**RFC-2026-000060**
1.  **ONE FILE, ONE CARD:** Every single source file in the `oss_sovereignty` tree must have a corresponding "Card" in the Forge Directives.
2.  **VERIFICATION:** Before compilation, every file must be individually verified (existence/integrity) by the Forge.
3.  **RATIONALE:** This ensures absolute accounting of every byte entering the system. It simulates the "Cobol Conversion" workflow where every line is a transaction.
