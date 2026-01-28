


MEMORY
{
    ITCM (xrw)      : ORIGIN = 0x00000000, LENGTH = 64K
    DTCM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
    RAM (xrw)       : ORIGIN = 0x24000000, LENGTH = 512K    
    SRAM1 (xrw)     : ORIGIN = 0x30000000, LENGTH = 128K    
    SRAM2 (xrw)     : ORIGIN = 0x30020000, LENGTH = 128K    
    SRAM3 (xrw)     : ORIGIN = 0x30040000, LENGTH = 32K     
    SRAM4 (xrw)     : ORIGIN = 0x38000000, LENGTH = 64K     
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 2048K   
    FLASH_EXT (rx)  : ORIGIN = 0x90000000, LENGTH = 16384K  
    FLASH_BL (rx)   : ORIGIN = 0x08000000, LENGTH = 128K    
    FLASH_FS (r)    : ORIGIN = 0x08020000, LENGTH = 128K    
    FLASH_TEXT (rx) : ORIGIN = 0x08040000, LENGTH = 1280K   
    FLASH_CM4 (rx)  : ORIGIN = 0x08180000, LENGTH = 512K    
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_estack = ORIGIN(RAM) + LENGTH(RAM) - _estack_reserve;
_sstack = _estack - 16K; 


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_heap_start = _ebss; 
_heap_end = _sstack;



_micropy_hw_internal_flash_storage_ram_cache_start = ORIGIN(DTCM);
_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(DTCM) + LENGTH(DTCM);


_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);


_openamp_shm_region_start = ORIGIN(SRAM4);
_openamp_shm_region_end = ORIGIN(SRAM4) + LENGTH(SRAM4);

INCLUDE common_blifs.ld
