 
 
#define SYM_FUNC_START_LOCAL(name)                      \
        SYM_START(name, SYM_L_GLOBAL, SYM_A_ALIGN)
#define memset MEMSET  
#define altinstr_replacement text
#define globl p2align 4; .globl
#include "../../arch/x86/lib/memset_64.S"

 
.section .note.GNU-stack,"",@progbits
