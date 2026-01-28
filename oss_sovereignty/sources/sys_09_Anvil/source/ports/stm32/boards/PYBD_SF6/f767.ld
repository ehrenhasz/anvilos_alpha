


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 2048K
    FLASH_ISR (rx)  : ORIGIN = 0x08000000, LENGTH = 32K     
    FLASH_APP (rx)  : ORIGIN = 0x08008000, LENGTH = 2016K   
    FLASH_EXT (rx)  : ORIGIN = 0x90000000, LENGTH = 2048K   
    RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 512K    
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 24K;


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;

INCLUDE common_bl.ld
