#ifndef _ASM_RISCV_PGTABLE_BITS_H
#define _ASM_RISCV_PGTABLE_BITS_H
#define _PAGE_ACCESSED_OFFSET 6
#define _PAGE_PRESENT   (1 << 0)
#define _PAGE_READ      (1 << 1)     
#define _PAGE_WRITE     (1 << 2)     
#define _PAGE_EXEC      (1 << 3)     
#define _PAGE_USER      (1 << 4)     
#define _PAGE_GLOBAL    (1 << 5)     
#define _PAGE_ACCESSED  (1 << 6)     
#define _PAGE_DIRTY     (1 << 7)     
#define _PAGE_SOFT      (1 << 8)     
#define _PAGE_SPECIAL   _PAGE_SOFT
#define _PAGE_TABLE     _PAGE_PRESENT
#define _PAGE_PROT_NONE _PAGE_GLOBAL
#define _PAGE_SWP_EXCLUSIVE _PAGE_ACCESSED
#define _PAGE_PFN_SHIFT 10
#define _PAGE_LEAF (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)
#endif  
