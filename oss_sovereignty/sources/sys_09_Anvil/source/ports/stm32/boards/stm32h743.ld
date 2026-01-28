


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1536K   
    FLASH_APP  (rx) : ORIGIN = 0x08020000, LENGTH = 1408K   
    FLASH_FS (r)    : ORIGIN = 0x08180000, LENGTH = 512K    
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


_micropy_hw_internal_flash_storage_ram_cache_start = ORIGIN(DTCM);
_micropy_hw_internal_flash_storage_ram_cache_end = ORIGIN(DTCM) + LENGTH(DTCM);


_micropy_hw_internal_flash_storage_start = ORIGIN(FLASH_FS);
_micropy_hw_internal_flash_storage_end = ORIGIN(FLASH_FS) + LENGTH(FLASH_FS);


SECTIONS
{
    .eth_buffers (NOLOAD) : {
        . = ABSOLUTE(0x30040000);
        *eth.o*(.bss.eth_dma)
    } >RAM_D2
}
