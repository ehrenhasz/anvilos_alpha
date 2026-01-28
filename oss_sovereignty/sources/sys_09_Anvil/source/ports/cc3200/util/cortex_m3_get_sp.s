

    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .text
    .align  2

    .global cortex_m3_get_sp
    .type cortex_m3_get_sp, %function

@ uint cortex_m3_get_sp(void)
cortex_m3_get_sp:
    @ return the sp
    mov     r0, sp
    bx      lr

    .size cortex_m3_get_sp, .-cortex_m3_get_sp
