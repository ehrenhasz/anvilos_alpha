 

 

 
#define SYM_FUNC_START_LOCAL(name)                      \
        SYM_START(name, SYM_L_GLOBAL, SYM_A_ALIGN)
#define memcpy MEMCPY  
#define altinstr_replacement text
#define globl p2align 4; .globl
#define _ASM_EXTABLE_FAULT(x, y)
#define _ASM_EXTABLE(x, y)

#include "../../arch/x86/lib/memcpy_64.S"
 
.section .note.GNU-stack,"",@progbits
