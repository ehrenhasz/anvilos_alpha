



_flashsize = 512K;  
_bootloader = 16K;  


MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00004000, LENGTH = _codesize
    RAM (xrw)   : ORIGIN = 0x20000000, LENGTH = 192K
}


_estack = ORIGIN(RAM) + LENGTH(RAM) - 8;
_sstack = _estack - 16K;

_oflash_fs = ORIGIN(FLASH) + _codesize;
_sflash_fs = _flashsize - _codesize - _bootloader;

_sheap = _ebss;
_eheap = _sstack;
