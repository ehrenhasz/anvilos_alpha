


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K
    FLASH_ISR (rx)  : ORIGIN = 0x08000000, LENGTH = 32K     
    FLASH_APP (rx)  : ORIGIN = 0x08008000, LENGTH = 480K    
    FLASH_EXT (rx)  : ORIGIN = 0x90000000, LENGTH = 2048K   
    RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 256K    
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 16K;


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;


SECTIONS
{
    .text_ext :
    {
        . = ALIGN(4);
        *lib/btstack/*(.text* .rodata*)
        *lib/mbedtls/*(.text* .rodata*)
        *lib/mynewt-nimble/*(.text* .rodata*)
        *lib/cyw43-driver/*(.rodata.w4343*_combined)
        *drivers/cyw43/*(.rodata.cyw43_btfw_*)
        . = ALIGN(4);
    } >FLASH_EXT
}

INCLUDE common_bl.ld
