PROJECT ANVIL: TACTICAL DEPLOYMENT PROTOCOL
TARGET: TYPE-0 HYPERVISOR (HARDENED SELINUX/GENTOO)
AUTHORIZATION: ALPHA-ONE
STATUS: CLASSIFIED

[SHE] Cracks knuckles.

Listen up. If you're choosing the Gentoo Hardened path for Project Anvil, you aren't just installing an OS. You are choosing pain, but it is the correct kind of pain. You are building a fortress from the ground up to cage a god.

Standard "Handbook" installs are for hobbyists. We are building a userspace specifically designed to wrap around a custom Anvil kernel's security features like a straitjacket.

Here is the "Line-by-Line" tactical outline for building the Gentoo Hardened SELinux Hypervisor Base.

================================================================================ PHASE 1: THE FOUNDATION (THE STAGE3)

We do not touch standard ISOs. We do not touch standard profiles. We start with the Hardened Stage3 because time is a resource and compiling GCC with hardened flags from scratch takes 48 hours of it.

BOOT STRATEGY:

Boot a minimal Live Environment (Gentoo Minimal CD preferred).

Verify network connectivity immediately. You need a stable pipe to the mirrors.

DISK PREPARATION (CRITICAL):

Wipe the drive. No trace of previous filesystems.

Partition Scheme:

/boot (EFI/BIOS): 512MB

/ (Root): Remainder.

Formatting:
$ mkfs.xfs -i size=512 /dev/sdaX
OR
$ mkfs.ext4 -O security /dev/sdaX

!IMPORTANT!: You MUST verify that the filesystem supports Extended Attributes (xattrs) natively. If you fail this, SELinux labels will not stick, and the system will be dead on arrival.

ACQUISITION:

Navigate to Gentoo mirrors: releases/amd64/autobuilds/current-stage3-amd64-hardened-selinux-openrc/

Target File: stage3-amd64-hardened-selinux-openrc-*.tar.xz

Verify the GPG signature. Trust nothing you didn't sign yourself, but trust the Gentoo infra keys for now.

DEPLOYMENT (THE EXTRACTION):

Mount your root partition to /mnt/gentoo.

Command:
$ tar xpvf stage3-.tar.xz --xattrs-include='.*' --numeric-owner -C /mnt/gentoo

LINE-BY-LINE ANALYSIS:

xpvf: Extract, preserve permissions, verbose, file.

--xattrs-include='*.*': This is the kill switch. This flag preserves the extensive SELinux security labels embedded in the archive. Without this, you are just extracting files; with it, you are extracting a security policy.

--numeric-owner: Ensures UID/GIDs match the Anvil spec, not the live CD's potentially random mapping.

================================================================================ PHASE 2: THE PROFILE (THE BLUEPRINT)

Once you chroot into the environment, you must tell Portage that this is an Anvil SELinux system.

SYNC:
$ emerge-webrsync
(Use the snapshot. It’s faster and crypto-verified).

PROFILE SELECTION:
$ eselect profile list

Scan for: default/linux/amd64/23.0/no-multilib/hardened/selinux

$ eselect profile set <number>

EFFECT: This modifies the entire build graph. Every package explicitly pulled from this point forward will be linked against libselinux and compiled with PIE/SSP enforcement.

================================================================================ PHASE 3: YOUR KERNEL (THE HEART OF ANVIL)

You are building the Anvil kernel from source. This is not a generic kernel; it is a hostile environment for exploits.

SOURCES:
$ emerge --ask sys-kernel/gentoo-sources

THE CONFIGURATION RITUAL (.config checklist):
You must enable these, or the userspace will panic on boot.

[SECURITY]
CONFIG_SECURITY_SELINUX=y
CONFIG_SECURITY_SELINUX_BOOTPARAM=y
CONFIG_SECURITY_SELINUX_DISABLE=n  <-- CRITICAL: Prevent runtime disabling
CONFIG_AUDIT=y
CONFIG_DEFAULT_SECURITY_SELINUX=y

[HARDENING]
CONFIG_LSM="lockdown,yama,loadpin,safesetid,integrity,selinux,bpf,landlock"
CONFIG_PAGE_POISONING=y
CONFIG_SLAB_FREELIST_HARDENED=y
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y
CONFIG_BUG_ON_DATA_CORRUPTION=y

DEPLOYMENT:
Compile and copy your kernel image to /boot.
$ make && make modules_install
$ cp arch/x86/boot/bzImage /boot/vmlinuz-anvil

================================================================================ PHASE 4: THE USERSPACE (THE MUSCLES)

We install tools to make the system "Network Ready" but "Shell Only." No X11. No Wayland. Just pure functionality.

CORE UTILITIES:
$ emerge --ask sys-kernel/linux-firmware  # Only if you need blobs
$ emerge --ask app-admin/sysklogd         # Required for logging SELinux denials
$ emerge --ask sys-process/cronie         # Scheduler

NETWORK STACK:
We keep it stripped down.
$ emerge --ask net-misc/dhcpcd
$ rc-update add dhcpcd default

SELINUX POLICY ENGINE:
$ emerge --ask sys-apps/policycoreutils
$ emerge --ask sec-policy/selinux-base-policy

================================================================================ PHASE 5: THE LABELING (THE RITUAL)

This makes Anvil unique. We must label the filesystem before the first boot, or the kernel will reject access to /sbin/init.

CONFIGURATION:
Edit /etc/selinux/config

SELINUX=permissive (Trust me. Boot once to log denials, then lock it down).

SELINUXTYPE=mcs (Multi-Category Security. Required for VM isolation).

RELABELING:
$ rlpkg -a -r

This command reads the binary policy and writes security contexts to every inode on the drive. It defines what is "System" and what is "Target."

================================================================================ PHASE 6: THE BOOTLOADER

The bootloader is the final gatekeeper.

INSTALL:
$ emerge --ask sys-boot/grub

CONFIGURE (/etc/default/grub):
We pass the security parameters directly to the kernel ring 0.

GRUB_CMDLINE_LINUX="lsm=lockdown,yama,bpf security=selinux selinux=1 slab_nomerge init_on_alloc=1 init_on_free=1 page_poison=1"

DEPLOY:
$ grub-install /dev/sda
$ grub-mkconfig -o /boot/grub/grub.cfg

================================================================================ THE FINAL HANDOVER

Exit chroot.

Unmount /mnt/gentoo.

Reboot.

[SHE] If you executed this protocol correctly, you will land at a login prompt.
Run: $ sestatus

If it reads "SELinux status: enabled", you have won. You have built the Anvil.
Now, edit /etc/selinux/config to SELINUX=enforcing and reboot one last time to lock the door and throw away the key.

Dismissed.
