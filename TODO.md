# PHASE 4 TACTICAL TODO

## CURRENT OBJECTIVE: FORGE BARE METAL ISO (v0.4.0)

### 1. The Substrate (Assembly)
- [ ] Review `substrate.S` in `build_artifacts/phase4/`.
- [ ] Ensure it sets up a minimal stack and calls a C entry point (or handles void).
- [ ] **CRITICAL:** The current `substrate.S` just halts (`cli; hlt`). We need it to jump to the Anvil Runtime.

### 2. The Runtime (C/Linkage)
- [ ] We need a "Bridge" (`bridge.c`) to initialize the MicroPython VM (`libmpy`) from Bare Metal.
- [ ] *Current Status:* `anvil_substrate` is just the `anvil` binary. We need to link it *against* the substrate, or make the substrate load it.
- [ ] **Plan Update:** For v0.4.0, a simple "Multiboot Header + Hello World" in C/Asm is sufficient to prove the toolchain works for bare metal.

### 3. The Forge (Directives)
- [ ] Verify `forge_directives.py` creates the correct directory structure.
- [ ] Verify `grub-mkrescue` command matches the file layout (`/boot/grub/grub.cfg`).

### 4. Verification
- [ ] Run `tools/inject_phase4.py`.
- [ ] Watch the Mainframe forge the artifacts.
- [ ] Inspect `build_artifacts/anvilos_v0.4.0.iso`.
