.section .text
.global _start
_start:
    /* Minimal x86_64 Bare Metal Entry */
    cli
    hlt
