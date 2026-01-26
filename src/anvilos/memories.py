# Anvilos System Memories (Replaces GEMINI.md)

MEMORIES = [
    {
        "topic": "Build Progress",
        "fact": "The AnvilOS Alpha ISO build is in progress, initiated via a comprehensive sequence of Anvil recipes injected into the Mainframe. The final artifact will be 'anvilos-alpha.iso'."
    },
    {
        "topic": "Codebase Reorganization",
        "fact": "The AnvilOS codebase has been reorganized into a 'src' based structure. All DND domain logic (dnd_dm, the_butcher, library_archivist, frontend) has been removed. Core system files (AIMEAT, Big Iron, Cortex, Interceptor) are now generic and use dynamic path resolution. 'runtime' is deprecated in favor of 'src/anvilos'."
    },
    {
        "topic": "Phase 2 Injection",
        "fact": "Fixed tools/inject_phase2.py to use 'src' path and local 'data/cortex.db'. Kernel fetch/config succeeded. Build in progress."
    },
    {
        "topic": "ZFS Build Knowledge",
        "fact": "Critical build constraints discovered: 1) ZFS requires static 'zlib', 'libuuid', 'libblkid', and 'libtirpc' (for RPC) in the build environment. 2) 'libtool' requires ABSOLUTE paths for 'DESTDIR' during 'make install'. 3) ZFS 2.2.2 has a duplicate symbol 'highbit64' conflict with 'zstream' that requires patching to 'zstream_highbit64'."
    },
    {
        "topic": "Operational Protocol",
        "fact": "AIMEAT (The Operator) must remain 'hands off' unless explicitly asked. Workflow: User -> Anvil -> Mainframe -> Forge (3 retries) -> Error Report -> User Fix."
    }
]
