

MEMORY
{
    FLASH_APP (rx)  : ORIGIN = 0x08020000, LENGTH = 1920K   
    FLASH_QSPI (rx) : ORIGIN = 0x90000000, LENGTH = 64M     
    DTCM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K    
    RAM (xrw)       : ORIGIN = 0x20020000, LENGTH = 384K    
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 32K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;

ENTRY(Reset_Handler)

REGION_ALIAS("FLASH_COMMON", FLASH_APP);

SECTIONS
{
    
    .text_qspi :
    {
        . = ALIGN(4);
        *extmod/*(.text* .rodata*)
        . = ALIGN(4);
    } >FLASH_QSPI

    INCLUDE common_isr.ld
    INCLUDE common_text.ld
    INCLUDE common_extratext_data_in_flash.ld
    INCLUDE common_bss_heap_stack.ld
}
