ENTRY(Reset_Handler)


SECTIONS
{
    
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector)) 
        *(.text)             
        *(.text*)            
        *(.rodata)           
        *(.rodata*)          
          
          

        . = ALIGN(4);
        _etext = .;          
    } >FLASH_TEXT

    

    
    _sidata = LOADADDR(.data);

    
    .data :
    {
        . = ALIGN(4);
        _sdata = .;        
        _ram_start = .;    
        *(.data)           
        *(.data*)          

        . = ALIGN(4);
        _edata = .;        
    } >RAM AT>FLASH_TEXT

    
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;         
        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;         
    } >RAM

    
    

    .ARM.attributes 0 : { *(.ARM.attributes) }
}


_ram_end = ORIGIN(RAM) + LENGTH(RAM);
_estack = ORIGIN(RAM) + LENGTH(RAM);
_unused_flash_start = (_sidata + (_edata - _sdata));
_unused_flash_len = (ORIGIN(FLASH_TEXT) + LENGTH(FLASH_TEXT)) - _unused_flash_start;
