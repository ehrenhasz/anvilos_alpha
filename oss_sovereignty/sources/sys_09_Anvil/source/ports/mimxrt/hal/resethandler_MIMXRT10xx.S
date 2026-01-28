









    .syntax unified
    .arch armv7-m

#include"flexram_config.s"




    .thumb_func
    .align 2
    .globl   Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    cpsid   i               
    .equ    VTOR, 0xE000ED08
    ldr     r0, =VTOR
    ldr     r1, =__isr_vector
    str     r1, [r0]
    ldr     r2, [r1]
    msr     msp, r2


    dsb
    isb
    ldr r0, =__iomux_gpr17_adr   
    ldr r1, =__iomux_gpr17_value 
    str r1,[r0]                  
    dsb
    isb
#if defined MIMXRT117x_SERIES
    ldr r0, =__iomux_gpr18_adr   
    ldr r1, =__iomux_gpr18_value 
    str r1,[r0]                  
    dsb
    isb
#endif
    ldr r0, =__iomux_gpr16_adr  
    ldr r1,[r0]                 
    orr r1, r1, #4              
    str r1,[r0]                 
    dsb
    isb

#ifndef __NO_SYSTEM_INIT
    ldr   r0,=SystemInit
    blx   r0
#endif


    ldr    r1, =__etext
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__

#ifdef __PERFORMANCE_IMPLEMENTATION

    subs    r3, r2
    ble    .LC1
.LC0:
    subs    r3, #4
    ldr    r0, [r1, r3]
    str    r0, [r2, r3]
    bgt    .LC0
.LC1:
#else  
.LC0:
    cmp     r2, r3
    ittt    lt
    ldrlt   r0, [r1], #4
    strlt   r0, [r2], #4
    blt    .LC0
#endif
#ifdef __STARTUP_INITIALIZE_RAMFUNCTION
    ldr    r2, =__ram_function_start__
    ldr    r3, =__ram_function_end__
#ifdef __PERFORMANCE_IMPLEMENTATION

    subs    r3, r2
    ble    .LC_ramfunc_copy_end
.LC_ramfunc_copy_start:
    subs    r3, #4
    ldr    r0, [r1, r3]
    str    r0, [r2, r3]
    bgt    .LC_ramfunc_copy_start
.LC_ramfunc_copy_end:
#else  
.LC_ramfunc_copy_start:
    cmp     r2, r3
    ittt    lt
    ldrlt   r0, [r1], #4
    strlt   r0, [r2], #4
    blt    .LC_ramfunc_copy_start
#endif
#endif 
#ifdef __STARTUP_INITIALIZE_NONCACHEDATA
    ldr    r2, =__noncachedata_start__
    ldr    r3, =__noncachedata_init_end__
#ifdef __PERFORMANCE_IMPLEMENTATION

    subs    r3, r2
    ble    .LC3
.LC2:
    subs    r3, #4
    ldr    r0, [r1, r3]
    str    r0, [r2, r3]
    bgt    .LC2
.LC3:
#else  
.LC2:
    cmp     r2, r3
    ittt    lt
    ldrlt   r0, [r1], #4
    strlt   r0, [r2], #4
    blt    .LC2
#endif

    ldr r3, =__noncachedata_end__
    movs    r0,0
.LC4:
    cmp    r2,r3
    itt    lt
    strlt   r0,[r2],#4
    blt    .LC4
#endif 

#ifdef __STARTUP_CLEAR_BSS

    ldr r1, =__bss_start__
    ldr r2, =__bss_end__

    movs    r0, 0
.LC5:
    cmp     r1, r2
    itt    lt
    strlt   r0, [r1], #4
    blt    .LC5
#endif 

    cpsie   i               
#ifndef __START
#define __START _start
#endif
#ifndef __ATOLLIC__
    ldr   r0,=__START
    blx   r0
#else
    ldr   r0,=__libc_init_array
    blx   r0
    ldr   r0,=main
    bx    r0
#endif
    .pool
    .size Reset_Handler, . - Reset_Handler
