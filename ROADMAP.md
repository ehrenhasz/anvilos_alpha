# PROJECT ANVILOS_ALPHA: SOVEREIGN ROADMAP

> **PRIME DIRECTIVE:** The Anvil is Law.
> **CURRENT PHASE:** **PHASE 4 (HYBRID SOVEREIGNTY)**.
> **STRATEGY:** Merge the stability of the Linux Monolith (Phase 3) with the Sovereign Law of the Unikernel (Phase 4).

## PHASE 1: THE MAINFRAME (ORCHESTRATION) [COMPLETE]
- [x] **Cortex:** `cortex.db` schema active (Stack, Telemetry).
- [x] **Daemons:** `architect_daemon` and `processor_daemon` active.
- [x] **Protocol:** MicroJSON (RFC-0058) established.

## PHASE 2: THE MONOLITH (INFRASTRUCTURE) [COMPLETE]
- [x] **Kernel:** Linux 6.6.14 (Sovereign Build).
- [x] **Userland:** Busybox 1.36.1 (Static) + ZFS 2.2.2 (Static).
- [x] **Artifact:** `build_artifacts/anvilos.iso` (154MB).

## PHASE 3: THE TRANSMUTATION (POPULATION) [COMPLETE]
- [x] **Law:** Python source (`.py`) transmuted to Anvil Bytecode (`.anv`).
- [x] **Population:** 19 Sovereign Artifacts embedded in ISO.

## PHASE 4: HYBRID SOVEREIGNTY (THE MERGE) [ACTIVE]
**Objective:** Forge a new stable Linux Monolith base from scratch and embed the Anvil Law runtime.
- [ ] **Step 1 (The Clean Slate):** Re-build the Linux Monolith (Kernel + GNU Userland) from scratch using the Sovereign Toolchain.
- [ ] **Step 2 (The Bridge):** Create a userspace launcher (`/bin/anvil`) for the Anvil Runtime.
- [ ] **Step 3 (Expansion):** Add requested "Normal Shell" tools to the Monolith (Bash 5.2, Coreutils, Vim full) to replace the minimal Busybox environment.
- [ ] **Step 4 (Integration):** Boot to Bash, but allow `anvil start` to drop into the Sovereign Environment.

## PHASE 5: THE CRYSTAL PALACE (PURE PYTHON USERLAND) [PENDING]
**Objective:** A window manager and user environment written 100% in Anvil Law, running ON TOP of the Phase 4 Hybrid Kernel.

---
**LEGACY ARCHIVE:**
* "Bare Metal Unikernel" (Zero-C) experiment archived as `v0.4.x` failures. Requires bare-metal IDT/GDT implementation to resume.
