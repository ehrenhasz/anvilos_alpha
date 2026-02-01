# AnvilOS Alpha - Official Roadmap

This roadmap outlines the major objectives for building a minimal, bootable, ZFS-based ISO while adhering strictly to the "Anvil is Law" protocol.

## Objective 0: Source Code Sovereignty & Sanitization
Prepare all third-party source code by stripping it down to a minimal, clean, and sovereign base. This involves removing unnecessary architectures, documentation, localizations, and features, and preparing it for an eventual rewrite into pure Anvil code.

## Objective 1: Establish the Sovereign Build System
Create a master build system where all orchestration and compilation is handled by scripts that are themselves compiled to bytecode and executed by the sovereign `anvil` runtime. No "dirty" host Python will be used in the build process.

## Objective 2: Build Core Components via the Sovereign System
Using the established build system, compile all necessary software components from the source code located in `oss_sovereignty`. This includes the Linux Kernel, a core userland (e.g., coreutils, bash), ZFS, and OpenSSH.

## Objective 3: Assemble the Final ISO
Package all compiled artifacts into a bootable `anvilos.iso`. This involves creating a root filesystem, building a ZFS-aware initramfs, and using a bootloader like GRUB to create the final image.