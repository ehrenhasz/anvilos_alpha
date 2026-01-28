


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 2048K
    FLASH_ISR (rx)  : ORIGIN = 0x08000000, LENGTH = 128K    
    FLASH_FS (r)    : ORIGIN = 0x08020000, LENGTH = 128K    
    FLASH_TEXT (rx) : ORIGIN = 0x08040000, LENGTH = 1792K   
    DTCM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K    
    RAM (xrw)       : ORIGIN = 0x24000000, LENGTH = 512K    
    RAM_D2 (xrw)    : ORIGIN = 0x30000000, LENGTH = 288K
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 16K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;
