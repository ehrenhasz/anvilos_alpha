# Anvil Sovereign System: Intelligence Report

**Status:** COMPILED FROM ARTIFACT ANALYSIS
**Date:** 2026-01-30

## 1. Core Philosophy ("The Law")
*   **"Anvil is Law":** Code executed by the host's standard interpreters (e.g., `/usr/bin/python3`, `/usr/bin/gcc`) is considered "Dirty", "Prototype", or "Illegal".
*   **Sovereign Execution:** Only code executed by the Sovereign Runtime (`/usr/local/bin/anvil`) or compiled by the Sovereign Toolchain (`x86_64-bicameral-linux-musl-gcc`) is considered "Law".
*   **Hermetic Seal:** The build system must not rely on host libraries (glibc, systemd). It must be self-contained.

## 2. The Languages of Anvil
There are two distinct "dialects" referred to as Anvil, serving different purposes:

### A. Anvil Systems Language (`.anv`)
*   **Purpose:** Low-level system components (Kernel modules, Drivers, Bare-metal logic).
*   **Syntax:** A **Rust-subset** (e.g., `fn`, `struct`, `impl`, `u8`, `unsafe`).
*   **Compilation Pipeline:**
    1.  **Transpilation:** `.anv` source is converted to ANSI C (`.c`) via the `anvil.py` transpiler.
    2.  **Compilation:** The resulting C code is compiled by the Sovereign GCC into a static binary or object file.
    3.  **Header mapping:** Maps Anvil types to C types (e.g., `u8` -> `uint8_t`) via `kernel.h`.
*   **Reference:** RFC-2026-000058 (The Law of Anvil).

### B. Anvil Scripting Language (`.py` / `.mpy`)
*   **Purpose:** High-level orchestration, Build scripts, Userland tools.
*   **Syntax:** **Python-compatible** (MicroPython subset).
*   **The Ritual (Mandatory Workflow):**
    1.  **Code:** Write logic in `logic.py`.
    2.  **Transmute:** `./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross logic.py` -> `logic.mpy`.
    3.  **Execute:** `/usr/local/bin/anvil logic.mpy`.
*   **Constraints:**
    *   **NO RAW PYTHON:** Running `.py` files directly on the host or even on `anvil` (as text) is considered "Dirty". Logic MUST be frozen bytecode.
    *   No heavy standard imports (`subprocess`, `requests`, `threading` are banned or replaced).
    *   File length limit: ~100 lines (The "Micro-Chunk Strategy").
*   **Reference:** RFC-2026-000009 (Coding in Anvil).

## 3. The Anvil Runtime (`/usr/local/bin/anvil`)
*   **Nature:** A customized, statically linked MicroPython binary (bicameral-vm).
*   **Capabilities:**
    *   Standard `import json`, `import os`, `import sys`.
    *   **Custom `anvil` module:** Provides specialized functionality like `anvil.check_output(cmd)` to safely execute shell commands (replacing `subprocess`).
    *   **Network:** Restricted. Must use "The Umbilical" for external access.

## 4. Protocols & Data
*   **MicroJSON (µJSON):** The mandatory format for internal logging and inter-agent communication.
    *   **Format:** `{"@ID": <INTEGER>, "data": <PAYLOAD>}`
    *   **Goal:** Eliminate the bloat of standard JSON keys.
    *   **Reference:** RFC-2026-000002.
*   **Artifacts:**
    *   Every Sovereign Artifact (binary or bytecode) must be accompanied by a **MicroJSON Sidecar**.
    *   Sidecars contain: Description, Hash ID (Checksum), and Metadata.

## 5. The Forge Workflow
1.  **Directives:** High-level tasks (Cards) from the Operator.
2.  **Recipe:** The Forge Agent converts directives into a `recipe.py` (Anvil Script).
3.  **Transmutation:** The `recipe.py` is compiled into `recipe.mpy`.
4.  **Mainframe:** The `anvil` runtime executes the `recipe.mpy` to produce the final artifact.
5.  **Verification:** The "Hold" rejects any artifact without a valid MicroJSON sidecar and checksum.

## 6. The Correct Anvil Instruction
To execute any logic as "Law", follow this exact sequence:

```bash
# 1. Compile to Bytecode
./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross <script>.py

# 2. Run on Sovereign Runtime
/usr/local/bin/anvil <script>.mpy
```

## 7. Current Conflict (Vim Source)
*   **Issue:** The user rejected `anvil_forge.py` as "not Anvil".
*   **Reason:** `anvil_forge.py` was being run as a raw Python script. To comply with "Anvil is Law", all build logic must be transmuted to `.mpy` before execution.
*   **Resolution Requirement:** All future build orchestration (including `build_phase4_anvil.py`) must be compiled to `.mpy` and run via the static `anvil` runtime.
