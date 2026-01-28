

    .syntax unified
    .cpu cortex-m3
    .fpu softvfp
    .thumb

    .section .text.Reset_Handler
    .global Reset_Handler
    .type Reset_Handler, %function

Reset_Handler:
    
    mov  r4, r0

    
    ldr  r0, =_estack
    mov  sp, r0

    
    ldr  r1, =_sidata
    ldr  r2, =_sdata
    ldr  r3, =_edata
    b    .data_copy_entry
.data_copy_loop:
    ldr  r0, [r1]
    adds r1, #4
    str  r0, [r2]
    adds r2, #4
.data_copy_entry:
    cmp  r2, r3
    bcc  .data_copy_loop

    
    movs r0, #0
    ldr  r1, =_sbss
    ldr  r2, =_ebss
    b    .bss_zero_entry
.bss_zero_loop:
    str  r0, [r1]
    adds r1, #4
.bss_zero_entry:
    cmp  r1, r2
    bcc  .bss_zero_loop

    
    mov  r0, r4
    bl   stm32_main

    .size Reset_Handler, .-Reset_Handler
