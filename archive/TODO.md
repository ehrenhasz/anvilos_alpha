# AnvilOS Hybrid Phase 4 TODO

## IMMEDIATE ACTIONS
- [x] **Sovereign Bootstrap:** Re-forge the `/usr/local/bin/anvil` binary using the Sovereign Toolchain, adding static support for `subprocess` or custom execution wrappers to allow Anvil to drive the build process without host Python.
- [ ] **Verify Base:** Confirm `build_artifacts/anvilos.iso` (Phase 3 Monolith) is still present and bootable.
- [ ] **Cleanup:** Archive the broken `build_artifacts/phase4/` (Bare Metal) artifacts to `archive/phase4_fail/`.
- [ ] **Toolchain:** Ensure the Sovereign Toolchain is ready to build standard Linux userspace tools (Bash, Coreutils).

## USERLAND EXPANSION (The "Normal Shell")
- [x] **Fetch Sources:**
    - [x] Bash 5.2
    - [x] Coreutils 9.4
    - [x] Ncurses (Dependency)
    - [x] Readline (Dependency)
    - [x] Vim 9.1
    - [x] OpenSSL 3.2.1
- [x] **Forge:** Compile these statically using the Sovereign Toolchain.
- [x] **Integrate:** Pack them into the `rootfs` of the Linux Monolith.
- [ ] **Forge:** OpenSSH 9.6p1.

## THE BRIDGE (Anvil Runtime on Linux)
- [x] **Port:** Adapt the `anvil` MicroPython runtime (currently `ports/embed`) to run as a standard Linux executable (`ports/unix`).
- [ ] **Install:** Place `/usr/bin/anvil` in the ISO.
- [ ] **Test:** Ensure `anvil main.py` works from the Linux shell.
