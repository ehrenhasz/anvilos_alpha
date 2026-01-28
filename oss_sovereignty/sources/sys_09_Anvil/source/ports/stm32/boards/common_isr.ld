


.isr_vector :
{
    . = ALIGN(4);
    KEEP(*(.isr_vector)) 
    . = ALIGN(4);
} >FLASH_COMMON
