# ANVILOS MAINFRAME & FORGEMASTER MANUAL
## 1. THE MAINFRAME
The **Mainframe** is the central nervous system of AnvilOS. It acts as the "Source of Truth" for all system operations, enforcing sovereignty by requiring all modifications to occur through **Punch Cards** (structured JSON instructions) rather than direct shell access.
### 1.1 Components
*   **CortexDB (`cortex.db`)**: A SQLite database acting as the "Card Stack" (queue). It stores the state of every operation.
    *   **`card_stack` Table**: Stores the sequence of instructions (Cards).
    *   **`sys_jobs` Table**: Stores jobs from the Card Reader.
*   **Processor Daemon (`processor_daemon.py`)**: The executor. It polls the Card Stack for `PENDING` (Stat 0) cards, executes them, and updates their status to `SUCCESS` (Stat 2) or `FAILURE` (Stat 9).
*   **Architect Daemon (`architect_daemon.py`)**: The planner. It listens for high-level "Goals" and uses the Forgemaster (AI) to break them down into atomic Cards.
*   **Operator Interface (`aimeat.py`)**: The CLI tool for human operators to monitor status, inject cards, and resolve jams.
### 1.2 Card States
| Status Code | State      | Description                                      |
| :---        | :---       | :---                                             |
| **0**       | `PENDING`  | Awaiting execution by the Processor.             |
| **1**       | `PROCESSING`| Currently running.                              |
| **2**       | `PUNCHED`  | Successfully executed.                           |
| **9**       | `JAMMED`   | Execution failed. Requires Operator intervention.|
---
## 2. THE FORGEMASTER (CODER)
The **Forgemaster** (Agent ID: `CODER`) is the Sovereign Architect of AnvilOS. It is an autonomous AI agent responsible for writing the code and "recipes" that build the system.
### 2.1 Role
*   **Architect**: Interprets high-level objectives (e.g., "Build the Kernel") and generates precise, step-by-step Card sequences.
*   **Coder**: Writes the actual source code (C, Python, Rust) to be injected into the system.
*   **Safety**: The Forgemaster is bound by **The Laws of Anvil** (RFCs), ensuring all code is auditable, reproducible, and contained within the `ascii_oss` directory structure.
### 2.2 Interaction
The Forgemaster is invoked by the `architect_daemon.py` or manually via `coder.py`. It does not execute code; it *writes* code to be executed by the Mainframe.
---
## 3. OPERATIONAL GUIDELINES (FOR OPERATORS)
### 3.1 Monitoring
Use the Operator Interface to check system health:
```bash
python3 aimeat.py status
```
This displays the current Queue, Active Card, and Recent Failures.
### 3.2 Resolving Jams (Status 9)
When a card jams:
1.  **Analyze**: Check the `ret` (return payload) for the error message.
2.  **Correct**:
    *   If it's a transient error, you may clone the card and retry.
    *   If it's a code error, inject a `FILE_WRITE` card to fix the source, then retry the build.
    *   If it's a resource issue (OOM), patch the pending cards to reduce concurrency (e.g., `-j2`).
3.  **Inject**: Use `aimeat.py` or `inject_*.py` scripts to submit corrective actions.
### 3.3 Resource Management
*   **Concurrency**: To prevent "Run Away Ram" (OOM), all build operations (`make`, `cargo build`) must be limited (e.g., `-j2`).
*   **VM Sizing**: Firecracker instances should be restricted to 50% of available host RAM.
---
## 4. DIRECTORY STRUCTURE (ASCII_OSS)
The Forgemaster operates strictly within the `ascii_oss` hierarchy to ensure supply chain sovereignty:
```
ascii_oss/
├── $TOOL_NAME/
│   ├── origin/      (Upstream source, dirty)
│   ├── sovereign/   (Forked, clean source)
│   ├── buildroot/   (Temporary build artifacts)
│   └── MANIFEST.md  (Audit log)
```