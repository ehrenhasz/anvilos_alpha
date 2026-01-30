.section .multiboot
.align 4
.long 0x1BADB002
.long 0x0
.long -(0x1BADB002 + 0x0)

.section .text
.global _start
.code32
_start:
    /* Setup Stack at 2MB (Safe Zone) */
    mov $0x200000, %esp  
    and $0xFFFFFFF0, %esp /* 16-byte alignment for GCC */
    call main_c
    cli
    hlt
