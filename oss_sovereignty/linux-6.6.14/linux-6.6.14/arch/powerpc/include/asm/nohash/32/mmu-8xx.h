#ifndef _ASM_POWERPC_MMU_8XX_H_
#define _ASM_POWERPC_MMU_8XX_H_
#define SPRN_MI_CTR	784	 
#define MI_GPM		0x80000000	 
#define MI_PPM		0x40000000	 
#define MI_CIDEF	0x20000000	 
#define MI_RSV4I	0x08000000	 
#define MI_PPCS		0x02000000	 
#define MI_IDXMASK	0x00001f00	 
#define SPRN_MI_AP	786
#define MI_Ks		0x80000000	 
#define MI_Kp		0x40000000	 
#define MI_APG_INIT	0xde000000
#define SPRN_MI_EPN	787
#define MI_EPNMASK	0xfffff000	 
#define MI_EVALID	0x00000200	 
#define MI_ASIDMASK	0x0000000f	 
#define SPRN_MI_TWC	789
#define MI_APG		0x000001e0	 
#define MI_GUARDED	0x00000010	 
#define MI_PSMASK	0x0000000c	 
#define MI_PS8MEG	0x0000000c	 
#define MI_PS512K	0x00000004	 
#define MI_PS4K_16K	0x00000000	 
#define MI_SVALID	0x00000001	 
#define SPRN_MI_RPN	790
#define MI_SPS16K	0x00000008	 
#define MI_BOOTINIT	0x000001fd
#define SPRN_MD_CTR	792	 
#define MD_GPM		0x80000000	 
#define MD_PPM		0x40000000	 
#define MD_CIDEF	0x20000000	 
#define MD_WTDEF	0x10000000	 
#define MD_RSV4I	0x08000000	 
#define MD_TWAM		0x04000000	 
#define MD_PPCS		0x02000000	 
#define MD_IDXMASK	0x00001f00	 
#define SPRN_M_CASID	793	 
#define MC_ASIDMASK	0x0000000f	 
#define SPRN_MD_AP	794
#define MD_Ks		0x80000000	 
#define MD_Kp		0x40000000	 
#define MD_APG_INIT	0xdc000000
#define MD_APG_KUAP	0xde000000
#define SPRN_MD_EPN	795
#define MD_EPNMASK	0xfffff000	 
#define MD_EVALID	0x00000200	 
#define MD_ASIDMASK	0x0000000f	 
#define SPRN_M_TWB	796
#define	M_L1TB		0xfffff000	 
#define M_L1INDX	0x00000ffc	 
#define SPRN_MD_TWC	797
#define MD_L2TB		0xfffff000	 
#define MD_L2INDX	0xfffffe00	 
#define MD_APG		0x000001e0	 
#define MD_GUARDED	0x00000010	 
#define MD_PSMASK	0x0000000c	 
#define MD_PS8MEG	0x0000000c	 
#define MD_PS512K	0x00000004	 
#define MD_PS4K_16K	0x00000000	 
#define MD_WT		0x00000002	 
#define MD_SVALID	0x00000001	 
#define SPRN_MD_RPN	798
#define MD_SPS16K	0x00000008	 
#define SPRN_M_TW	799
#if defined(CONFIG_PPC_4K_PAGES)
#define mmu_virtual_psize	MMU_PAGE_4K
#elif defined(CONFIG_PPC_16K_PAGES)
#define mmu_virtual_psize	MMU_PAGE_16K
#define PTE_FRAG_NR		4
#define PTE_FRAG_SIZE_SHIFT	12
#define PTE_FRAG_SIZE		(1UL << 12)
#else
#error "Unsupported PAGE_SIZE"
#endif
#define mmu_linear_psize	MMU_PAGE_8M
#define MODULES_VADDR	(PAGE_OFFSET - SZ_256M)
#define MODULES_END	PAGE_OFFSET
#ifndef __ASSEMBLY__
#include <linux/mmdebug.h>
#include <linux/sizes.h>
void mmu_pin_tlb(unsigned long top, bool readonly);
typedef struct {
	unsigned int id;
	unsigned int active;
	void __user *vdso;
	void *pte_frag;
} mm_context_t;
#define PHYS_IMMR_BASE (mfspr(SPRN_IMMR) & 0xfff80000)
#define VIRT_IMMR_BASE (__fix_to_virt(FIX_IMMR_BASE))
struct mmu_psize_def {
	unsigned int	shift;	 
	unsigned int	enc;	 
	unsigned int    ind;     
	unsigned int	flags;
#define MMU_PAGE_SIZE_DIRECT	0x1	 
#define MMU_PAGE_SIZE_INDIRECT	0x2	 
};
extern struct mmu_psize_def mmu_psize_defs[MMU_PAGE_COUNT];
static inline int shift_to_mmu_psize(unsigned int shift)
{
	int psize;
	for (psize = 0; psize < MMU_PAGE_COUNT; ++psize)
		if (mmu_psize_defs[psize].shift == shift)
			return psize;
	return -1;
}
static inline unsigned int mmu_psize_to_shift(unsigned int mmu_psize)
{
	if (mmu_psize_defs[mmu_psize].shift)
		return mmu_psize_defs[mmu_psize].shift;
	BUG();
}
static inline bool arch_vmap_try_size(unsigned long addr, unsigned long end, u64 pfn,
				      unsigned int max_page_shift, unsigned long size)
{
	if (end - addr < size)
		return false;
	if ((1UL << max_page_shift) < size)
		return false;
	if (!IS_ALIGNED(addr, size))
		return false;
	if (!IS_ALIGNED(PFN_PHYS(pfn), size))
		return false;
	return true;
}
static inline unsigned long arch_vmap_pte_range_map_size(unsigned long addr, unsigned long end,
							 u64 pfn, unsigned int max_page_shift)
{
	if (arch_vmap_try_size(addr, end, pfn, max_page_shift, SZ_512K))
		return SZ_512K;
	if (PAGE_SIZE == SZ_16K)
		return SZ_16K;
	if (arch_vmap_try_size(addr, end, pfn, max_page_shift, SZ_16K))
		return SZ_16K;
	return PAGE_SIZE;
}
#define arch_vmap_pte_range_map_size arch_vmap_pte_range_map_size
static inline int arch_vmap_pte_supported_shift(unsigned long size)
{
	if (size >= SZ_512K)
		return 19;
	else if (size >= SZ_16K)
		return 14;
	else
		return PAGE_SHIFT;
}
#define arch_vmap_pte_supported_shift arch_vmap_pte_supported_shift
extern s32 patch__itlbmiss_exit_1, patch__dtlbmiss_exit_1;
extern s32 patch__itlbmiss_perf, patch__dtlbmiss_perf;
#endif  
#endif  
