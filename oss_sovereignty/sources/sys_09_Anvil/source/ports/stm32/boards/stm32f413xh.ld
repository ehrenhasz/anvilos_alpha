


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1536K 
    FLASH_ISR (rx)  : ORIGIN = 0x08000000, LENGTH = 16K 
    FLASH_FS (rx)   : ORIGIN = 0x08004000, LENGTH = 176K 
    FLASH_FS2 (rx)  : ORIGIN = 0x08040000, LENGTH = 64K 
    FLASH_TEXT (rx) : ORIGIN = 0x08060000, LENGTH = 1152K 
    SRAM2 (xrw)     : ORIGIN = 0x10000000, LENGTH = 64K 
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 256K
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 16K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;


_micropy_hw_internal_flash_storage_ram_cache_start = ORIGIN(SRAM2);
_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(SRAM2) + LENGTH(SRAM2);
_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);
_micropy_hw_internal_flash_storage2_start = ORIGIN(FLASH_FS2);
_micropy_hw_internal_flash_storage2_end = ORIGIN(FLASH_FS2) + LENGTH(FLASH_FS2);
