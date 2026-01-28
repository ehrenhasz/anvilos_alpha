


_minimum_stack_size = 8K;


_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
    
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector)) 
        . = ALIGN(4);
    } >FLASH_BL

    
    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH_BL

    
    _sidata = LOADADDR(.data);

    
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)

        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH_BL

    
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    
    .nozero_bss (NOLOAD) :
    {
        . = ALIGN(4);
        *(.nozero_bss*)
        . = ALIGN(4);
    } >RAM

    
    .stack :
    {
        . = ALIGN(4);
        . = . + _minimum_stack_size;
        . = ALIGN(4);
    } >RAM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
