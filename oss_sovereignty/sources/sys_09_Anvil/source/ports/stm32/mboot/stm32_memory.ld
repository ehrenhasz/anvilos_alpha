

MEMORY
{
    FLASH_BL (rx)   : ORIGIN = 0x08000000, LENGTH = 32K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 120K
}


_mboot_protected_flash_start = ORIGIN(FLASH_BL);
_mboot_protected_flash_end_exclusive = ORIGIN(FLASH_BL) + LENGTH(FLASH_BL);
