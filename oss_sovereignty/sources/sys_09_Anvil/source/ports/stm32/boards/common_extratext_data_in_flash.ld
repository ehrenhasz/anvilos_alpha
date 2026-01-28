


.ARM :
{
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
} >FLASH_COMMON


_sidata = LOADADDR(.data);


.data :
{
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
} >RAM AT> FLASH_COMMON
