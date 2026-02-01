
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>

#include <asm/cpu_entry_area.h>
#include <asm/fixmap.h>
#include <asm/e820/api.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <linux/vmalloc.h>

unsigned int __VMALLOC_RESERVE = 128 << 20;

  
void set_pte_vaddr(unsigned long vaddr, pte_t pteval)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	p4d = p4d_offset(pgd, vaddr);
	if (p4d_none(*p4d)) {
		BUG();
		return;
	}
	pud = pud_offset(p4d, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_none(pteval))
		set_pte_at(&init_mm, vaddr, pte, pteval);
	else
		pte_clear(&init_mm, vaddr, pte);

	 
	flush_tlb_one_kernel(vaddr);
}

unsigned long __FIXADDR_TOP = 0xfffff000;
EXPORT_SYMBOL(__FIXADDR_TOP);

 
static int __init parse_vmalloc(char *arg)
{
	if (!arg)
		return -EINVAL;

	 
	__VMALLOC_RESERVE = memparse(arg, &arg) + VMALLOC_OFFSET;
	return 0;
}
early_param("vmalloc", parse_vmalloc);

 
static int __init parse_reservetop(char *arg)
{
	unsigned long address;

	if (!arg)
		return -EINVAL;

	address = memparse(arg, &arg);
	reserve_top_address(address);
	early_ioremap_init();
	return 0;
}
early_param("reservetop", parse_reservetop);
