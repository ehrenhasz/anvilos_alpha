# AnvilOS Hybrid Phase 4 TODO

## IMMEDIATE ACTIONS
- [ ] **Verify Base:** Confirm `build_artifacts/anvilos.iso` (Phase 3 Monolith) is still present and bootable.
- [ ] **Cleanup:** Archive the broken `build_artifacts/phase4/` (Bare Metal) artifacts to `archive/phase4_fail/`.
- [ ] **Toolchain:** Ensure the Sovereign Toolchain is ready to build standard Linux userspace tools (Bash, Coreutils).

## USERLAND EXPANSION (The "Normal Shell")
- [ ] **Fetch Sources:**
    - [ ] Bash 5.2
    - [ ] Coreutils 9.4
    - [ ] Ncurses (Dependency)
    - [ ] Readline (Dependency)
- [ ] **Forge:** Compile these statically using the Sovereign Toolchain.
- [ ] **Integrate:** Pack them into the `rootfs` of the Linux Monolith.

## THE BRIDGE (Anvil Runtime on Linux)
- [ ] **Port:** Adapt the `anvil` MicroPython runtime (currently `ports/embed`) to run as a standard Linux executable (`ports/unix`).
- [ ] **Install:** Place `/usr/bin/anvil` in the ISO.
- [ ] **Test:** Ensure `anvil main.py` works from the Linux shell.
