

ENTRY(Reset_Handler)

REGION_ALIAS("FLASH_COMMON", FLASH_TEXT);


SECTIONS
{
    
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector)) 

        

        . = ALIGN(4);

        

        */ff.o(.text*)
        */vfs_fat_*.o(.text*)
        */py/formatfloat.o(.text*)
        */py/parsenum.o(.text*)
        */py/mpprint.o(.text*)

        . = ALIGN(4);
    } >FLASH_ISR

    INCLUDE common_text.ld
    INCLUDE common_extratext_data_in_flash.ld
    INCLUDE common_bss_heap_stack.ld
}
