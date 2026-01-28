#ifndef _XTENSA_TLB_H
#define _XTENSA_TLB_H
#include <asm/cache.h>
#include <asm/page.h>
#include <asm-generic/tlb.h>
#define __pte_free_tlb(tlb, pte, address)	pte_free((tlb)->mm, pte)
void check_tlb_sanity(void);
#endif	 
