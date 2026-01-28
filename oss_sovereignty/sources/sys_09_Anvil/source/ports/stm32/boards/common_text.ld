


.text :
{
    . = ALIGN(4);
    *(.text*)          
    *(.rodata*)        
    
    

    . = ALIGN(4);
    _etext = .;        
} >FLASH_COMMON
