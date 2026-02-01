# Anvil Coding Manual

**Version:** 1.0  
**Status:** ACTIVE  
**Audience:** Sovereign Developers, AI Agents

---

## 1. Introduction

This manual defines the standard operating procedure for writing, compiling, and executing logic within the AnvilOS Sovereign Environment. Unlike traditional systems where source code is often unstructured text files (like `.py` or `.c`), AnvilOS enforces a strict, structured, and compiled workflow to ensure hermeticity and security.

## 2. Core Concepts & Terminology

The Anvil ecosystem redefines standard file extensions to distinguish between "Human Intent" and "Machine Reality".

### 2.1 File Extensions

| Extension | Name | Description |
| :--- | :--- | :--- |
| **`.mpy`** | **MicroJSON Source** | **Human-Readable Source.** The authoritative source of truth. It is a structured JSON file containing the high-level logic (Python syntax embedded), metadata, and safety maps. It is *not* a raw text file. |
| **`.anv`** | **AI Machine Byte Code** | **Machine-Executable Binary.** The result of compiling a `.mpy` file. It is an opaque binary blob of opcodes. It is immutable and cannot be reverse-engineered back to source by the runtime. |

### 2.2 The Sovereign Constraint ("The Law")

*   **No Direct Execution:** You cannot run `.mpy` files directly. They must be compiled.
*   **No "Dirty" Tools:** You must not use host Python or GCC. All compilation happens via the `anvil` toolchain.
*   **Structure over Text:** Code must be wrapped in the MicroJSON structure.

---

## 3. The Development Workflow

The lifecycle of an Anvil program is:

1.  **Drafting:** Create a `.mpy` (MicroJSON) file defining your module.
2.  **Compilation:** Use the `anvil` compiler to freeze the JSON into `.anv` bytecode.
3.  **Execution:** The Anvil Runtime (`vm`) executes the `.anv` artifact.

### Step 1: Writing Source (`.mpy`)

A valid `.mpy` file is a JSON object. It is NOT a compiled bytecode file (contrary to standard MicroPython usage).

**Template:**
```json
{
  "module_name": "my_module",
  "hash_id": "unique_sha256_hash",
  "original_prompt": "Description of what this code does",
  "human_readable_source": "print('Hello from AnvilOS')",
  "logic_map": {
    "variable_name": "Description of variable purpose"
  },
  "crash_correlation_map": {
    "0xERR": "Explanation of potential crash"
  }
}
```

**Key Field:** `human_readable_source` contains the actual Python-syntax logic.

### Step 2: Compilation

Use the sovereign `anvil` tool to transform your JSON source into Machine Code.

```bash
# Syntax: anvil build <source.mpy> -o <output.anv>
$ anvil build hello.mpy -o hello.anv
```

*   **Input:** structured JSON (`.mpy`)
*   **Process:** Extracts `human_readable_source`, strips comments/metadata, compiles to bytecode.
*   **Output:** Binary artifact (`.anv`)

### Step 3: Execution

The Anvil Runtime loads the `.anv` file.

```bash
$ vm hello.anv
>> Hello from AnvilOS
```

---

## 4. Example: Hello World

### 4.1 Source (`hello.mpy`)
```json
{
    "hash_id": "c66f3d75b887cf8144b610e3d24fed66bf23e6e6763656e7509498ca6ec96aff",
    "module_name": "hello_world",
    "original_prompt": "write the classic hello world in our new python langauge",
    "human_readable_source": "# This script prints a classic greeting.\nmessage = 'Hello, Fucking World!'\nprint(message)",
    "logic_map": {
        "message": "Variable holding the greeting string."
    },
    "failure_modes": ["None anticipated."],
    "crash_correlation_map": {}
}
```

### 4.2 Compiled Machine Code (`hello.anv`)
*(Representation of the binary output)*
```hex
4d 06 02 1f 40 00 00 00 1a 08 00 10 ...
```

---

## 5. Best Practices

1.  **Always update the `logic_map`**: When you add variables, document them in the JSON structure, not just in code comments (which are stripped).
2.  **Versioning**: Use the `hash_id` to track changes to the logic.
3.  **Validation**: Ensure your JSON is valid before attempting compilation. The `anvil` tool will reject malformed JSON.
