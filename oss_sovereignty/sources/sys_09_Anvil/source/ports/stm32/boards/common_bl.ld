

ENTRY(Reset_Handler)

REGION_ALIAS("FLASH_COMMON", FLASH_APP);


SECTIONS
{
    INCLUDE common_isr.ld
    INCLUDE common_text.ld
    INCLUDE common_extratext_data_in_flash.ld
    INCLUDE common_bss_heap_stack.ld
}
