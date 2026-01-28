


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 192K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 20K
}


_minimum_stack_size = 2K;
_minimum_heap_size = 8K;

_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 4K;


_heap_start = _ebss;
_heap_end = _sstack;
