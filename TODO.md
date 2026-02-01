# AnvilOS Alpha - Actionable TODO

This file tracks the granular tasks required to complete the objectives outlined in `ROADMAP.md`.

## Objective 0: Source Code Sovereignty & Sanitization
- [x] **Task 0.0.1: Verify OSS Asset Inventory:** Scan `oss_sovereignty` and report the list of all cloned projects.
- [x] **Task 0.0.2: Purge Non-x86 Architectures:** Identify and script the removal of all non-x86 architecture code.
- [x] **Task 0.0.3: Purge Non-English Localization & Documentation:** Identify and script the removal of all non-manpage documentation and localization files.
- [x] **Task 0.0.4: Strip All Source Code Comments:** Devise and report a script-based strategy to remove comments from all source files.
- [x] **Task 0.0.5: Aggressive Feature Pruning (Analysis Only):** For each component, analyze and list its sub-features, then await instructions on which ones to remove.
- [x] **Task 0.0.6: Update Official Roadmap:** This task. (Will be marked as done).

## Phase: Sovereign Source Encapsulation (Mass Ingestion)
- [x] **Task 0.0.7: Tooling Development:** Create `tools/ingest_staging.py` capable of processing `oss_sovereignty_staging` into `oss_sovereignty` with a hard limit of 100 files per execution (stateful progress tracking).
- [x] **Task 0.0.8: Batch A (The Anvil):** Ingest `sys_09_Anvil` (~1,600 files) in ~16 chunks.
- [x] **Task 0.0.9: Batch B (Core Userland):** Ingest `bash`, `coreutils`, `readline`, `ncurses`, `openssh` (~4,000 files) in ~40 chunks.
- [x] **Task 0.0.10: Batch C (The Monolith):** Ingest `linux-6.6.14` and `zfs` (~58,000 files) in ~580 chunks.
- [x] **Task 0.0.11: Final Verification:** Audit strict one-to-one mapping between Source `.mpy` and Staging file.

## Phase: Sovereign Artifact Generation (Mass Compilation)
- [x] **Task 0.0.12: Tooling Development:** Create `tools/compile_mass.py` to process `.mpy` -> `.anv` in stateful chunks. Must handle C compilation failures gracefully (log & continue).
- [ ] **Task 0.0.13: Execution Loop:** Run the compilation tool iteratively until all ~63k `.mpy` files are attempted.
- [ ] **Task 0.0.14: Failure Analysis:** Review `compile_failures.log` to identify missing headers/configs required for Objective 2.

## ON  HOLD BELOW THIS LINE. HUMAN WILL LET YOU KNOW WHEN TO MOVE ON

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
