# AnvilOS Alpha - Actionable TODO

This file tracks the granular tasks required to complete the objectives outlined in `ROADMAP.md`.

## Objective 0: Source Code Sovereignty & Sanitization
- [ ] **Task 0.1: Verify OSS Asset Inventory:** Scan `oss_sovereignty` and report the list of all cloned projects.
- [ ] **Task 0.2: Purge Non-x86 Architectures:** Identify and script the removal of all non-x86 architecture code.
- [ ] **Task 0.3: Purge Non-English Localization & Documentation:** Identify and script the removal of all non-manpage documentation and localization files.
- [ ] **Task 0.4: Strip All Source Code Comments:** Devise and report a script-based strategy to remove comments from all source files.
- [ ] **Task 0.5: Aggressive Feature Pruning (Analysis Only):** For each component, analyze and list its sub-features, then await instructions on which ones to remove.
- [ ] **Task 0.6: Update Official Roadmap:** This task. (Will be marked as done).


## Objective 1: Establish the Sovereign Build System

- [ ] **Task 1.1: Create Master Build Script:** Author a new `tools/build.py`. Its sole job will be to orchestrate the build by compiling and executing "recipe" scripts. This master script itself must be run as `anvil tools/build.mpy`.
- [ ] **Task 1.2: Refactor Recipes:** Convert the logic within the old `forge_phase4.py` into a series of small, single-purpose Python scripts in a new `tools/recipes/` directory (e.g., `build_zlib.py`, `build_kernel.py`, `build_zfs.py`).
- [ ] **Task 1.3: Enforce Anvil Law:** The master script will iterate through the recipes, first compiling each one (`mpy-cross -o recipe.mpy recipe.py`) and then executing it (`anvil recipe.mpy`).

## Objective 2: Build Components via the Sovereign System

- [ ] **Task 2.1: Build Kernel:** Execute the `build_kernel.py` recipe to produce a `bzImage`.
- [ ] **Task 2.2: Build Core Userland:** Execute recipes to build `coreutils`, `bash`, `ncurses`, and `readline`.
- [ ] **Task 2.3: Build ZFS:** Execute the `build_zfs.py` recipe.
- [ ] **Task 2.4: Build SSH:** Execute a recipe to build and install OpenSSH.

## Objective 3: Assemble the Final ISO

- [ ] **Task 3.1: Assemble Rootfs:** Create a recipe to copy all compiled binaries and libraries into a clean `build/rootfs/` directory.
- [ ] **Task 3.2: Create Initramfs:** Create a recipe to build a minimal `initramfs` capable of loading ZFS kernel modules.
- [ ] **Task 3.3: Generate Bootable ISO:** The final recipe will use a tool like `grub-mkrescue` to package the kernel, rootfs, and initramfs into a bootable `anvilos.iso`.