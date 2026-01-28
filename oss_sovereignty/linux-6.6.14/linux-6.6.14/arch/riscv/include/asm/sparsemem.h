#ifndef _ASM_RISCV_SPARSEMEM_H
#define _ASM_RISCV_SPARSEMEM_H
#ifdef CONFIG_SPARSEMEM
#ifdef CONFIG_64BIT
#define MAX_PHYSMEM_BITS	56
#else
#define MAX_PHYSMEM_BITS	34
#endif  
#define SECTION_SIZE_BITS	27
#endif  
#endif  
