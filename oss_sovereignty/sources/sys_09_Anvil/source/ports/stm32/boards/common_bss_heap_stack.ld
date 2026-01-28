


.bss :
{
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
} >RAM


.heap :
{
    . = ALIGN(4);
    . = . + _minimum_heap_size;
    . = ALIGN(4);
} >RAM


.stack :
{
    . = ALIGN(4);
    . = . + _minimum_stack_size;
    . = ALIGN(4);
} >RAM


_bl_state = _estack;
