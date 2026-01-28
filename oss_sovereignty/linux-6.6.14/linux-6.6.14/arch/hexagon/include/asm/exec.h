#ifndef _ASM_EXEC_H
#define _ASM_EXEC_H
#define STACK_MASK (~7)
#define arch_align_stack(x) (x & STACK_MASK)
#endif  
