# PHASE 4: BARE METAL / UNIKERNEL (ZERO C SUBSTRATE)
# ANVIL IS LAW. THE HOLD.

PHASE4_DIRECTIVES = [
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "rm -rf build_artifacts/phase4 && mkdir -p build_artifacts/phase4/bin build_artifacts/phase4/law",
            "desc": "Initialize Phase 4 Workspace"
        },
        "seq": 1
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cp oss_sovereignty/sys_09_Anvil/build/anvil build_artifacts/phase4/bin/anvil_substrate",
            "desc": "Stage Sovereign Runtime"
        },
        "seq": 2
    },
    {
        "op": "FILE_WRITE",
        "pld": {
            "path": "build_artifacts/phase4/substrate.S",
            "content": ".section .multiboot\n.align 4\n.long 0x1BADB002\n.long 0x0\n.long -(0x1BADB002 + 0x0)\n\n.section .text\n.global _start\n.code32\n_start:\n    mov $0xb8000, %edi\n    movb $'A', (%edi)\n    movb $0x2f, 1(%edi)\n    cli\n    hlt\n",
            "desc": "Forge Zero-C Boot Entry (Multiboot)"
        },
        "seq": 3
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "ext/toolchain/bin/x86_64-unknown-linux-musl-gcc -static -nostdlib -Wl,-Ttext=0x100000 build_artifacts/phase4/substrate.S -o build_artifacts/phase4/bin/anvil_unikernel",
            "desc": "Compile Zero-C Substrate"
        },
        "seq": 4
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross src/anvilos/synapse.py -o build_artifacts/phase4/law/synapse.anv",
            "desc": "Transmute Synapse to Law"
        },
        "seq": 5
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "./oss_sovereignty/sys_09_Anvil/source/mpy-cross/build/mpy-cross oss_sovereignty/sys_09_Anvil/hello.py -o build_artifacts/phase4/law/init.anv",
            "desc": "Transmute Hello to Init Law"
        },
        "seq": 6
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "tar -czf build_artifacts/phase4/law.tar.gz -C build_artifacts/phase4/law .",
            "desc": "Pack Sovereign Law"
        },
        "seq": 7
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cat build_artifacts/phase4/bin/anvil_unikernel build_artifacts/phase4/law.tar.gz > build_artifacts/phase4/bin/anvilos_v0.4.0.bin",
            "desc": "Fuse Law into Substrate"
        },
        "seq": 8
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p build_artifacts/phase4/iso/boot/grub && cp build_artifacts/phase4/bin/anvilos_v0.4.0.bin build_artifacts/phase4/iso/boot/ && echo 'menuentry \"AnvilOS v0.4.0 (Zero C)\" { multiboot /boot/anvilos_v0.4.0.bin }' > build_artifacts/phase4/iso/boot/grub/grub.cfg",
            "desc": "Prepare ISO Structure"
        },
        "seq": 9
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "grub-mkrescue -o build_artifacts/anvilos_v0.4.0.iso build_artifacts/phase4/iso",
            "desc": "Forge Phase 4 Sovereign ISO"
        },
        "seq": 10
    }
]

FULL_STACK = PHASE4_DIRECTIVES
