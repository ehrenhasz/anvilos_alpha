#ifndef _ASM_ARC_PGTABLE_H
#define _ASM_ARC_PGTABLE_H
#include <linux/bits.h>
#include <asm/pgtable-levels.h>
#include <asm/pgtable-bits-arcv2.h>
#include <asm/page.h>
#include <asm/mmu.h>
#define	USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#ifndef __ASSEMBLY__
extern char empty_zero_page[PAGE_SIZE];
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))
extern pgd_t swapper_pg_dir[] __aligned(PAGE_SIZE);
#define HAVE_ARCH_UNMAPPED_AREA
#endif  
#endif
