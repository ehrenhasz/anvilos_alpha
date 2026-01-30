.section .multiboot
.align 4
.long 0x1BADB002
.long 0x0
.long -(0x1BADB002 + 0x0)

.section .text
.global _start
.code32
_start:
    mov $0xb8000, %edi
    movb $'A', (%edi)
    movb $0x2f, 1(%edi)
    cli
    hlt
