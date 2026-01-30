ENTRY(_start)

SECTIONS {
    /* GRUB loads the kernel at 1MB */
    . = 1M;

    .text : {
        /* Multiboot header MUST be early in the binary */
        KEEP(*(.multiboot))
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
