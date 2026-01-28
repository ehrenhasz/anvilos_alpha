



MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K 
    FLASH_APP (rx)  : ORIGIN = 0x08010000, LENGTH = 976K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 320K 
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 12K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;
