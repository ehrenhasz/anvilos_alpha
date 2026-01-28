#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H
#include <asm-generic/pgtable-nopmd.h>
#include <linux/bits.h>
#include <linux/const.h>
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))
#define MAX_POSSIBLE_PHYSMEM_BITS 34
#define _PAGE_PFN_MASK  GENMASK(31, 10)
#define _PAGE_NOCACHE		0
#define _PAGE_IO		0
#define _PAGE_MTMASK		0
#define _PAGE_CHG_MASK  (~(unsigned long)(_PAGE_PRESENT | _PAGE_READ |	\
					  _PAGE_WRITE | _PAGE_EXEC |	\
					  _PAGE_USER | _PAGE_GLOBAL))
#endif  
