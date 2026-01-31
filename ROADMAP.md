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

## Phase 4: Hybrid Sovereignty (The Merge) - IN PROGRESS
**Goal:** A bootable Linux (v6.6.14) ISO where the userland is orchestrated entirely by Anvil.
**Protocol:** THE GRAND TRANSMUTATION (RFC-2026-000009 & RFC-0058)

### The Mandate ("Anvil All The Way Down")
1.  **Build System (`.mpy`):**
    *   No raw Python scripts (`.py`) allowed for execution.
    *   All build logic must be written in Python, **transmuted** to bytecode (`.mpy`) via `mpy-cross`, and executed by the Sovereign Runtime (`/usr/local/bin/anvil`).
    *   *Target:* `forge.mpy` (The Master Builder).

2.  **System Components (`.anv`):**
    *   No raw C code (`.c`) allowed for system binaries (Init, Daemons, Utilities).
    *   All system logic must be written in the **Anvil Systems Language** (Rust-subset `.anv`), transpiled to C via `anvil.py`, and compiled by the Sovereign Toolchain.
    *   *Target:* `init.anv` (PID 1), `shim.anv`.

3.  **The Artifact:**
    *   A bootable ISO (`anvilos_v0.5.0.iso`) containing:
        *   Linux Kernel 6.6.14 (The Substrate).
        *   Anvil Runtime (The Law).
        *   GNU Userland (The Tools - Bash, Coreutils, etc., forged by Anvil).
        *   Anvil System Binaries (The Soul - Init, Configs).

### Current Status
*   [x] Sovereign Toolchain (GCC/Musl) Active.
*   [x] Sovereign Runtime (`/usr/local/bin/anvil`) Patched & Ready.
*   [ ] Build System Rewrite (Python -> `.mpy`).
*   [ ] System Init Rewrite (C -> `.anv`).
*   [ ] Full Userland Forge (Bash, Coreutils, etc.).

## Phase 5: The Crystal Palace (Future)
**Objective:** A window manager and user environment written 100% in Anvil Law, running ON TOP of the Phase 4 Hybrid Kernel.

---
**LEGACY ARCHIVE:**
* "Bare Metal Unikernel" (Zero-C) experiment archived as `v0.4.x` failures. Requires bare-metal IDT/GDT implementation to resume.
