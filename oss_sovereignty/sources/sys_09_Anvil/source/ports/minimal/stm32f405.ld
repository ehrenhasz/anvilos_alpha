


MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 0x100000 
    CCMRAM (xrw)    : ORIGIN = 0x10000000, LENGTH = 0x010000 
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 0x020000 
}


_estack = ORIGIN(RAM) + LENGTH(RAM);


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
        _sidata = _etext;  
    } >FLASH

    
    .data : AT ( _sidata )
    {
        . = ALIGN(4);
        _sdata = .;        
        *(.data)           
        *(.data*)          

        . = ALIGN(4);
        _edata = .;        
    } >RAM

    
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
