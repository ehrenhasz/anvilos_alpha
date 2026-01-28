


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K 
    FLASH_APP (rx)  : ORIGIN = 0x08008000, LENGTH = 480K 
    FLASH_FS (r)    : ORIGIN = 0x08080000, LENGTH = 256K 
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 192K 
    RAM2A (xrw)     : ORIGIN = 0x20030000, LENGTH = 10K  
    RAM2B (xrw)     : ORIGIN = 0x20038000, LENGTH = 10K  
}


_minimum_stack_size = 2K;
_minimum_heap_size = 16K;


_ram_start = ORIGIN(RAM);
_ram_end = ORIGIN(RAM) + LENGTH(RAM);

_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(RAM) + LENGTH(RAM);
_micropy_hw_internal_flash_storage_ram_cache_start = _micropy_hw_internal_flash_storage_ram_cache_end - 4K; 


_estack = _micropy_hw_internal_flash_storage_ram_cache_start - _estack_reserve;
_sstack = _estack - 16K;

_heap_start = _ebss; 
_heap_end = _sstack;

_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end   = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);

SECTIONS
{
    
    .ram2a_bss :
    {
        . = ALIGN(4);
        . = . + 64; 
        *rfcore.o(.bss.ipcc_mem_*)
        . = ALIGN(4);
    } >RAM2A

    
    .ram2b_bss :
    {
        . = ALIGN(4);
        *rfcore.o(.bss.ipcc_membuf_*)
        . = ALIGN(4);
    } >RAM2B
}
