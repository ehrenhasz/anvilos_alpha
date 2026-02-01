 

    .syntax unified
    .thumb

    .section .text
    .align 2

    .global gc_helper_get_regs_and_sp
    .type gc_helper_get_regs_and_sp, %function

@ This function will compile on processors like Cortex M0 that don't support
@ newer Thumb-2 instructions.

@ uint gc_helper_get_regs_and_sp(r0=uint regs[10])
gc_helper_get_regs_and_sp:
    @ store registers into given array
    str    r4, [r0, #0]
    str    r5, [r0, #4]
    str    r6, [r0, #8]
    str    r7, [r0, #12]
    mov    r1, r8
    str    r1, [r0, #16]
    mov    r1, r9
    str    r1, [r0, #20]
    mov    r1, r10
    str    r1, [r0, #24]
    mov    r1, r11
    str    r1, [r0, #28]
    mov    r1, r12
    str    r1, [r0, #32]
    mov    r1, r13
    str    r1, [r0, #36]

    @ return the sp
    mov    r0, sp
    bx     lr

    .size gc_helper_get_regs_and_sp, .-gc_helper_get_regs_and_sp
