


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K 
    FLASH_FS (rx)   : ORIGIN = 0x08064000, LENGTH = 112K 
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 81408
    FS_CACHE (xrw)  : ORIGIN = 0x20013e00, LENGTH = 512
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 16K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;


_micropy_hw_internal_flash_storage_ram_cache_start = ORIGIN(FS_CACHE);
_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(FS_CACHE) + LENGTH(FS_CACHE);
_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);
