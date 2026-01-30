# AnvilOS Hybrid Phase 4 TODO

## IMMEDIATE ACTIONS
- [ ] **Clean Slate:** Abandon existing `anvilos.iso` and prepare for a fresh build of the Monolith.
- [ ] **Cleanup:** Archive all `v0.4.x` (Bare Metal) and `v0.1.0` (Old Monolith) artifacts to `archive/legacy/`.
- [ ] **Manifest:** Define the new core components for the "Normal Shell" userland.

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