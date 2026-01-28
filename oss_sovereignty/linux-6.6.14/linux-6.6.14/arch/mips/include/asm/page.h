#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H
#include <spaces.h>
#include <linux/const.h>
#include <linux/kernel.h>
#include <asm/mipsregs.h>
#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT	12
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define PAGE_SHIFT	13
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT	14
#endif
#ifdef CONFIG_PAGE_SIZE_32KB
#define PAGE_SHIFT	15
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT	16
#endif
#define PAGE_SIZE	(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK	(~((1 << PAGE_SHIFT) - 1))
static inline unsigned int page_size_ftlb(unsigned int mmuextdef)
{
	switch (mmuextdef) {
	case MIPS_CONF4_MMUEXTDEF_FTLBSIZEEXT:
		if (PAGE_SIZE == (1 << 30))
			return 5;
		if (PAGE_SIZE == (1llu << 32))
			return 6;
		if (PAGE_SIZE > (256 << 10))
			return 7;  
		fallthrough;
	case MIPS_CONF4_MMUEXTDEF_VTLBSIZEEXT:
		return (PAGE_SHIFT - 10) / 2;
	default:
		panic("Invalid FTLB configuration with Conf4_mmuextdef=%d value\n",
		      mmuextdef >> 14);
	}
}
#ifdef CONFIG_MIPS_HUGE_TLB_SUPPORT
#define HPAGE_SHIFT	(PAGE_SHIFT + PAGE_SHIFT - 3)
#define HPAGE_SIZE	(_AC(1,UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#else  
#define HPAGE_SHIFT	({BUILD_BUG(); 0; })
#define HPAGE_SIZE	({BUILD_BUG(); 0; })
#define HPAGE_MASK	({BUILD_BUG(); 0; })
#define HUGETLB_PAGE_ORDER	({BUILD_BUG(); 0; })
#endif  
#include <linux/pfn.h>
extern void build_clear_page(void);
extern void build_copy_page(void);
#ifdef CONFIG_MIPS_AUTO_PFN_OFFSET
extern unsigned long ARCH_PFN_OFFSET;
# define ARCH_PFN_OFFSET	ARCH_PFN_OFFSET
#else
# define ARCH_PFN_OFFSET	PFN_UP(PHYS_OFFSET)
#endif
extern void clear_page(void * page);
extern void copy_page(void * to, void * from);
extern unsigned long shm_align_mask;
static inline unsigned long pages_do_alias(unsigned long addr1,
	unsigned long addr2)
{
	return (addr1 ^ addr2) & shm_align_mask;
}
struct page;
static inline void clear_user_page(void *addr, unsigned long vaddr,
	struct page *page)
{
	extern void (*flush_data_cache_page)(unsigned long addr);
	clear_page(addr);
	if (pages_do_alias((unsigned long) addr, vaddr & PAGE_MASK))
		flush_data_cache_page((unsigned long)addr);
}
struct vm_area_struct;
extern void copy_user_highpage(struct page *to, struct page *from,
	unsigned long vaddr, struct vm_area_struct *vma);
#define __HAVE_ARCH_COPY_USER_HIGHPAGE
#ifdef CONFIG_PHYS_ADDR_T_64BIT
  #ifdef CONFIG_CPU_MIPS32
    typedef struct { unsigned long pte_low, pte_high; } pte_t;
    #define pte_val(x)	  ((x).pte_low | ((unsigned long long)(x).pte_high << 32))
    #define __pte(x)	  ({ pte_t __pte = {(x), ((unsigned long long)(x)) >> 32}; __pte; })
  #else
     typedef struct { unsigned long long pte; } pte_t;
     #define pte_val(x) ((x).pte)
     #define __pte(x)	((pte_t) { (x) } )
  #endif
#else
typedef struct { unsigned long pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) } )
#endif
typedef struct page *pgtable_t;
typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) } )
typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) } )
#define pte_pgprot(x)	__pgprot(pte_val(x) & ~_PFN_MASK)
#define ptep_buddy(x)	((pte_t *)((unsigned long)(x) ^ sizeof(pte_t)))
static inline unsigned long ___pa(unsigned long x)
{
	if (IS_ENABLED(CONFIG_64BIT)) {
		return x < CKSEG0 ? XPHYSADDR(x) : CPHYSADDR(x);
	}
	if (!IS_ENABLED(CONFIG_EVA)) {
		return CPHYSADDR(x);
	}
	return x - PAGE_OFFSET + PHYS_OFFSET;
}
#define __pa(x)		___pa((unsigned long)(x))
#define __va(x)		((void *)((unsigned long)(x) + PAGE_OFFSET - PHYS_OFFSET))
#include <asm/io.h>
#define __pa_symbol_nodebug(x)	__pa(RELOC_HIDE((unsigned long)(x), 0))
#ifdef CONFIG_DEBUG_VIRTUAL
extern phys_addr_t __phys_addr_symbol(unsigned long x);
#else
#define __phys_addr_symbol(x)	__pa_symbol_nodebug(x)
#endif
#ifndef __pa_symbol
#define __pa_symbol(x)		__phys_addr_symbol((unsigned long)(x))
#endif
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
#define virt_to_pfn(kaddr)   	PFN_DOWN(virt_to_phys((void *)(kaddr)))
#define virt_to_page(kaddr)	pfn_to_page(virt_to_pfn(kaddr))
extern bool __virt_addr_valid(const volatile void *kaddr);
#define virt_addr_valid(kaddr)						\
	__virt_addr_valid((const volatile void *) (kaddr))
#define VM_DATA_DEFAULT_FLAGS	VM_DATA_FLAGS_TSK_EXEC
extern unsigned long __kaslr_offset;
static inline unsigned long kaslr_offset(void)
{
	return __kaslr_offset;
}
#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>
#endif  
