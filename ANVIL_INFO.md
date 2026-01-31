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
*   **Compilation Pipeline:**
    1.  **Transmutation:** Source `.py` files are compiled to Bytecode (`.mpy`) using `mpy-cross`.
    2.  **Execution:** The `.mpy` artifacts are executed by the `/usr/local/bin/anvil` runtime.
*   **Constraints:**
    *   No heavy standard imports (`subprocess`, `requests`, `threading` are banned or replaced).
    *   File length limit: ~100 lines (The "Micro-Chunk Strategy").
*   **Reference:** RFC-2026-000009 (Coding in Anvil).

## 3. The Anvil Runtime (`/usr/local/bin/anvil`)
*   **Nature:** A customized, statically linked MicroPython binary.
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
3.  **Mainframe:** The `anvil` runtime executes the recipe to produce the final artifact.
4.  **Verification:** The "Hold" rejects any artifact without a valid MicroJSON sidecar and checksum.

## 6. Current Conflict (Vim Source)
*   **Issue:** The user rejected `anvil_forge.py` as "not Anvil".
*   **Reason:** `anvil_forge.py` is likely running on **Host Python** (`python3`) and is raw source, not a transmuted `.mpy` artifact running on the `anvil` runtime.
*   **Resolution Requirement:** Build logic must be written in Anvil-compatible Python, potentially named `build.py` or similar, and executed via the Sovereign Runtime (`anvil build.py`), or fully transmuted into an `.mpy` artifact before execution.
