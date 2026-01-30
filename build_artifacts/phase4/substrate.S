.section .multiboot
.align 4
.long 0x1BADB002
.long 0x0
.long -(0x1BADB002 + 0x0)

.section .text
.global _start
.code32
_start:
    mov $0x90000, %esp  /* Setup Stack at 576KB */
    call main_c
    cli
    hlt
