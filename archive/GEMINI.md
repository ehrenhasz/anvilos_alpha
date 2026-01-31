# Anvil Core Protocol

**"Anvil is Law"**: All code must be executed by the Sovereign Runtime (`/usr/local/bin/anvil`) or compiled by the Sovereign Toolchain. Execution via host interpreters (e.g., `/usr/bin/python3`) is forbidden.

**Anvil Scripting Language (`.py` -> `.mpy`)**
- **Purpose**: High-level orchestration, build scripts, userland tools.
- **Syntax**: Python-compatible (MicroPython subset).
- **Workflow (Mandatory)**:
    1.  **Code**: Write script in a `.py` file.
    2.  **Transmute**: Compile the script to bytecode using `mpy-cross`. (`./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross <script>.py`)
    3.  **Execute**: Run the resulting `.mpy` file with the Sovereign Runtime. (`/usr/local/bin/anvil <script>.mpy`)
- **Constraint**: Raw Python execution is "Dirty". Logic MUST be frozen bytecode.

**Anvil Systems Language (`.anv`)**
- **Purpose**: Low-level components (drivers, kernel modules).
- **Syntax**: Rust-subset (`fn`, `struct`, `impl`).
- **Workflow**:
    1.  **Transpile**: `.anv` source is converted to C via `anvil.py`.
    2.  **Compile**: The C code is compiled by the Sovereign GCC.

**Frameworks Status**
- The "Forge" and "Mainframe" frameworks are temporarily on hold for a future build. All operations will be conducted via explicit Anvil commands.