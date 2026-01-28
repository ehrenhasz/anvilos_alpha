#ifndef __ASM_MMU_H
#define __ASM_MMU_H
#include <asm/cputype.h>
#define MMCF_AARCH32	0x1	 
#define USER_ASID_BIT	48
#define USER_ASID_FLAG	(UL(1) << USER_ASID_BIT)
#define TTBR_ASID_MASK	(UL(0xffff) << 48)
#ifndef __ASSEMBLY__
#include <linux/refcount.h>
#include <asm/cpufeature.h>
typedef struct {
	atomic64_t	id;
#ifdef CONFIG_COMPAT
	void		*sigpage;
#endif
	refcount_t	pinned;
	void		*vdso;
	unsigned long	flags;
} mm_context_t;
#define ASID(mm)	(atomic64_read(&(mm)->context.id) & 0xffff)
static inline bool arm64_kernel_unmapped_at_el0(void)
{
	return cpus_have_const_cap(ARM64_UNMAP_KERNEL_AT_EL0);
}
extern void arm64_memblock_init(void);
extern void paging_init(void);
extern void bootmem_init(void);
extern void __iomem *early_io_map(phys_addr_t phys, unsigned long virt);
extern void create_mapping_noalloc(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, pgprot_t prot);
extern void create_pgd_mapping(struct mm_struct *mm, phys_addr_t phys,
			       unsigned long virt, phys_addr_t size,
			       pgprot_t prot, bool page_mappings_only);
extern void *fixmap_remap_fdt(phys_addr_t dt_phys, int *size, pgprot_t prot);
extern void mark_linear_text_alias_ro(void);
extern bool kaslr_requires_kpti(void);
#define INIT_MM_CONTEXT(name)	\
	.pgd = init_pg_dir,
#endif	 
#endif
