


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 256K 
    FLASH_APP (rx)  : ORIGIN = 0x08000000, LENGTH = 232K 
    FLASH_FS (r)    : ORIGIN = 0x0803a000, LENGTH = 24K  
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 64K  
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);

_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(RAM) + LENGTH(RAM);
_micropy_hw_internal_flash_storage_ram_cache_start = _micropy_hw_internal_flash_storage_ram_cache_end - 2K; 


_estack = _micropy_hw_internal_flash_storage_ram_cache_start - _estack_reserve;
_sstack = _estack - 6K;

_heap_start = _ebss; 
_heap_end = _sstack;

_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end   = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);
