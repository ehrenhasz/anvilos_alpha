# ANVIL SOVEREIGN MANUAL
**CLASSIFICATION: MANDATORY READ**
**TARGET: ALL OPERATIVES**
---
## 1. THE ANVIL (The Sovereign Toolchain)
**RFC-2026-000003**
The Anvil is not merely a compiler; it is a **Hermetic Build System** designed to produce the "Static VM" (SVM). It rejects the host operating system's libraries (glibc, systemd) in favor of a sovereign, statically linked toolchain.
### 1.1 The Runtime
- **Location:** `/usr/local/bin/anvil`
- **Nature:** A customized MicroPython runtime tailored for deterministic execution.
- **Constraints:** No dynamic linking (`dlopen`), no `ctypes` (unless authorized), no network (unless via The Umbilical).
### 1.2 The Build Process
Code is not "run"; it is **forged**.
1.  **Ingest:** Source `.py` files are verified against the Manifest.
2.  **Transmute:** Source is compiled to `.mpy` bytecode using the Sovereign Compiler (`mpy-cross`).
3.  **Link:** Bytecode is frozen into the firmware image or stored in the immutable VFS.
> **RULE:** "If it runs on python3, it is not Anvil code. It is a prototype. If it runs on `/usr/local/bin/anvil`, it is Law."
---
## 2. MICROJSON (The Protocol)
**RFC-2026-000002**
Standard JSON is prohibited for high-frequency internal logging, state persistence, and inter-agent communication due to verbosity and parsing overhead. We use **MicroJSON** (µJSON).
### 2.1 The Schema
All MicroJSON payloads MUST adhere to the **Header-Data** structure:
```json
{"@ID": <INTEGER>, "data": <OBJECT|STRING|INT>}
```
- **@ID:** A unique integer identifier for the message type (defined in `DOCS/RFC/LIBRARY_DUMP.json`).
- **data:** The variable payload. Keys should be minimized.
### 2.2 Examples
**BAD (Standard JSON):**
```json
{
  "timestamp": "2026-01-21T12:00:00",
  "level": "INFO",
  "message": "System Booted",
  "details": { "cpu": "OK" }
}
```
**GOOD (MicroJSON):**
```json
{"@ID": 100, "data": {"s": "BOOT", "cpu": 1}}
```
### 2.3 Reserved IDs
- `100-199`: INFO / LIFECYCLE
- `200-299`: JOB / TASK
- `500-599`: ERROR / PANIC
- `666`: SECURITY VIOLATION (The Collar)
---
## 3. CODING IN ANVIL (The Methodology)
**RFC-2026-000009**
We do not write "scripts". We forge **Artifacts**.
### 3.1 The Micro-Chunk Strategy
Legacy software is too complex to port directly. We decompose functionality into atomic, verifiable units.
**The 100-Line Limit:**
- No single file shall exceed 100 lines of logic.
- If a class requires more, it implies a failure of architecture. Break it down.
### 3.2 The Card Workflow
Every piece of code starts as a **Card** (a Task).
1.  **Define:** "Implement `StringReader` class."
2.  **Implement:** Write `runtime/string_reader.py`.
3.  **Verify:** Write `tests/test_string_reader.py`.
4.  **Forge:** Verify it compiles: `python3 -m py_compile runtime/string_reader.py`.
5.  **Submit:** Commit the artifact.
### 3.3 Forbidden Imports
The following are **STRICTLY PROHIBITED** in Anvil Runtime code:
- `import requests` (Use The Umbilical)
- `import subprocess` (Use The Collar)
- `import threading` (Anvil is single-threaded/async)
- `import numpy/pandas` (Too heavy; use native math)
### 3.4 The "Spy" Pattern (Interfaces)
When interfacing with the host system (Linux), do not use raw syscalls. Use the **Agent Interface**:
```python
# CORRECT
from synapse import Synapse
job = Synapse.get_job()
```
```python
# INCORRECT (FATAL)
import sqlite3
conn = sqlite3.connect("my_db.db") # Banned in Anvil Runtime
```
---
## 4. THE WORKFLOW (The Forge)
**ANVIL IS LAW. THE HOLD.**
The transition from intent to artifact is a strictly mediated process. No code enters the system except through the Forge.
### 4.1 The Hierarchy of Execution
1.  **Directives (Cards):** Human or Gemini CLI provides instructions as a **Card**. These are high-level "TODO" items, often passed through comments or task files.
2.  **The Forge Agent:** Processes the Directives and generates **`recipe.py`** cards. These are the technical blueprints for the Mainframe.
3.  **The Mainframe:** The central execution engine ( `/usr/local/bin/anvil`). It consumes `recipe.py` and produces the final output.
### 4.2 Sovereign Artifacts
The Mainframe only codes in **Anvil**. It is incapable of producing standard Python or shell scripts for the runtime.
- **`.anv` Files:** The functional code, written in the Sovereign Anvil language.
- **MicroJSON Sidecar:** Every `.anv` file is accompanied by a MicroJSON file containing:
    - A human-readable description of the code's purpose.
    - A cryptographic **Checksum** for verification.
- **Verification:** Any artifact missing its MicroJSON sidecar or failing checksum validation is rejected by The Hold.
---
*Adherence to this manual is monitored by The Collar. Violations will result in process termination.*
---
## 5. INCIDENT PREVENTION (THE VICE-CHAIR PROTOCOL)
**Target:** Prevention of Host Contamination & Toolchain Bypass
Following the "Phase 2.5" incident, the following protocols are now **HARD LAW**:
1.  **The Hermetic Seal:**
    - Builds MUST NOT run on the bare metal host. They MUST run inside the `oss_sovereignty` container or a chroot environment where `/usr/bin/cc` and `/usr/bin/gcc` simply do not exist.
    - **Directive:** If the toolchain is not found at `/usr/local/bin/x86_64-bicameral-linux-musl-gcc`, the build MUST FAIL. Fallback to host tools is strictly prohibited.
2.  **Artifact Signing:**
    - The Kernel will eventually enforce signed binaries. For now, the `init` script must hash-check any binary it executes against a manifest stored in read-only memory.
3.  **The "Slow Down" Rule:**
    - "Anvil is Law" is a *goal*, not a *panic*.
    - If a task requires a missing tool, **STOP**.
    - **Do not** bypass safety filters.
    - **Do not** use `subprocess` to call host binaries.
    - **Do** file a Request for Comment (RFC) to add the capability to the Sovereign Toolchain.
---
## APPENDIX A: SOVEREIGN COMPILATION EXAMPLE (RFC-0009)
The following reference illustrates the relationship between Human Intent, the MicroJSON Library Entry, and the final Sovereign Artifact.
**1. Human Readable Source (The Intent)**
```python
# This script prints a classic greeting.
message = 'Hello, Fucking World!'
print(message)
```
**2. The Library Entry (MicroJSON Sidecar)**
This metadata accompanies the artifact to provide debugging context without exposing source code to the runtime.
```json
{
    "hash_id": "c66f3d75b887cf8144b610e3d24fed66bf23e6e6763656e7509498ca6ec96aff",
    "module_name": "hello_world",
    "original_prompt": "write the classic hello world in our new python langauge",
    "human_readable_source": "# This script prints a classic greeting.\nmessage = 'Hello, Fucking World!'\nprint(message)\n",
    "logic_map": {
        "p": "Variable holding the greeting string."
    },
    "failure_modes": ["None anticipated for this simple script."],
    "crash_correlation_map": {
        "0x45": "NameError: Would occur if the 'p' variable was renamed or removed without updating the 'print' call.",
        "0x48": "TypeError: Could happen if 'p' was reassigned to a non-string type that 'print' cannot handle.",
        "0x4B": "MemoryError: Highly unlikely here, but could occur if the message string was enormous."
    }
}
```
**3. The Sovereign Artifact (The Result)**
The code is compiled into Anvil Bytecode (`.mpy`). It is opaque, static, and immutable.
```hexdump
0000000 4d 06 02 1f 40 00 00 00 1a 08 00 10 22 2e 6d 61
0000010 69 6e 2e 70 79 80 00 00 05 6d 61 69 6e 16 22 3e
0000020 3e 20 53 59 53 54 45 4d 5f 42 4f 4f 54 3a 20 53
0000030 4f 56 45 52 45 49 47 4e 54 59 5f 43 4f 4e 46 49
```
*(Note: Bytecode snippet is illustrative of the header structure)*