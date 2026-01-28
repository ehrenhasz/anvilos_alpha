#ifndef __ASM_ARC_PAGE_H
#define __ASM_ARC_PAGE_H
#include <uapi/asm/page.h>
#ifdef CONFIG_ARC_HAS_PAE40
#define MAX_POSSIBLE_PHYSMEM_BITS	40
#define PAGE_MASK_PHYS			(0xff00000000ull | PAGE_MASK)
#else  
#define MAX_POSSIBLE_PHYSMEM_BITS	32
#define PAGE_MASK_PHYS			PAGE_MASK
#endif  
#ifndef __ASSEMBLY__
#define clear_page(paddr)		memset((paddr), 0, PAGE_SIZE)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)
#define copy_page(to, from)		memcpy((to), (from), PAGE_SIZE)
struct vm_area_struct;
struct page;
#define __HAVE_ARCH_COPY_USER_HIGHPAGE
void copy_user_highpage(struct page *to, struct page *from,
			unsigned long u_vaddr, struct vm_area_struct *vma);
void clear_user_page(void *to, unsigned long u_vaddr, struct page *page);
typedef struct {
	unsigned long pgd;
} pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) })
#if CONFIG_PGTABLE_LEVELS > 3
typedef struct {
	unsigned long pud;
} pud_t;
#define pud_val(x)      	((x).pud)
#define __pud(x)        	((pud_t) { (x) })
#endif
#if CONFIG_PGTABLE_LEVELS > 2
typedef struct {
	unsigned long pmd;
} pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) })
#endif
typedef struct {
#ifdef CONFIG_ARC_HAS_PAE40
	unsigned long long pte;
#else
	unsigned long pte;
#endif
} pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) })
typedef struct {
	unsigned long pgprot;
} pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })
#define pte_pgprot(x)	__pgprot(pte_val(x))
typedef struct page *pgtable_t;
#define virt_to_pfn(kaddr)	(__pa(kaddr) >> PAGE_SHIFT)
#ifdef CONFIG_HIGHMEM
extern unsigned long arch_pfn_offset;
#define ARCH_PFN_OFFSET		arch_pfn_offset
extern int pfn_valid(unsigned long pfn);
#define pfn_valid		pfn_valid
#else  
#define ARCH_PFN_OFFSET		virt_to_pfn((void *)CONFIG_LINUX_RAM_BASE)
#endif  
#define __pa(vaddr)  		((unsigned long)(vaddr))
#define __va(paddr)  		((void *)((unsigned long)(paddr)))
#define virt_to_page(kaddr)	pfn_to_page(virt_to_pfn(kaddr))
#define virt_addr_valid(kaddr)  pfn_valid(virt_to_pfn(kaddr))
#define VM_DATA_DEFAULT_FLAGS	VM_DATA_FLAGS_NON_EXEC
#define WANT_PAGE_VIRTUAL   1
#include <asm-generic/memory_model.h>    
#include <asm-generic/getorder.h>
#endif  
#endif
