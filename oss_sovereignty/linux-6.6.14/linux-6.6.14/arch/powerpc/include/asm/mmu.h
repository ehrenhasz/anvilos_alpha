#ifndef _ASM_POWERPC_MMU_H_
#define _ASM_POWERPC_MMU_H_
#ifdef __KERNEL__
#include <linux/types.h>
#include <asm/asm-const.h>
#define MMU_FTR_HPTE_TABLE		ASM_CONST(0x00000001)
#define MMU_FTR_TYPE_8xx		ASM_CONST(0x00000002)
#define MMU_FTR_TYPE_40x		ASM_CONST(0x00000004)
#define MMU_FTR_TYPE_44x		ASM_CONST(0x00000008)
#define MMU_FTR_TYPE_FSL_E		ASM_CONST(0x00000010)
#define MMU_FTR_TYPE_47x		ASM_CONST(0x00000020)
#define MMU_FTR_TYPE_RADIX		ASM_CONST(0x00000040)
#define MMU_FTR_KUAP		ASM_CONST(0x00000200)
#define MMU_FTR_BOOK3S_KUEP		ASM_CONST(0x00000400)
#define MMU_FTR_PKEY			ASM_CONST(0x00000800)
#define MMU_FTR_GTSE			ASM_CONST(0x00001000)
#define MMU_FTR_68_BIT_VA		ASM_CONST(0x00002000)
#define MMU_FTR_KERNEL_RO		ASM_CONST(0x00004000)
#define MMU_FTR_TLBIE_CROP_VA		ASM_CONST(0x00008000)
#define MMU_FTR_USE_HIGH_BATS		ASM_CONST(0x00010000)
#define MMU_FTR_BIG_PHYS		ASM_CONST(0x00020000)
#define MMU_FTR_USE_TLBIVAX_BCAST	ASM_CONST(0x00040000)
#define MMU_FTR_USE_TLBILX		ASM_CONST(0x00080000)
#define MMU_FTR_LOCK_BCAST_INVAL	ASM_CONST(0x00100000)
#define MMU_FTR_NEED_DTLB_SW_LRU	ASM_CONST(0x00200000)
#define MMU_FTR_NO_SLBIE_B		ASM_CONST(0x02000000)
#define MMU_FTR_16M_PAGE		ASM_CONST(0x04000000)
#define MMU_FTR_TLBIEL			ASM_CONST(0x08000000)
#define MMU_FTR_LOCKLESS_TLBIE		ASM_CONST(0x10000000)
#define MMU_FTR_CI_LARGE_PAGE		ASM_CONST(0x20000000)
#define MMU_FTR_1T_SEGMENT		ASM_CONST(0x40000000)
#define MMU_FTR_NX_DSI			ASM_CONST(0x80000000)
#define MMU_FTRS_DEFAULT_HPTE_ARCH_V2	(MMU_FTR_HPTE_TABLE | MMU_FTR_TLBIEL | MMU_FTR_16M_PAGE)
#define MMU_FTRS_POWER		MMU_FTRS_DEFAULT_HPTE_ARCH_V2
#define MMU_FTRS_PPC970		MMU_FTRS_POWER | MMU_FTR_TLBIE_CROP_VA
#define MMU_FTRS_POWER5		MMU_FTRS_POWER | MMU_FTR_LOCKLESS_TLBIE
#define MMU_FTRS_POWER6		MMU_FTRS_POWER5 | MMU_FTR_KERNEL_RO | MMU_FTR_68_BIT_VA
#define MMU_FTRS_POWER7		MMU_FTRS_POWER6
#define MMU_FTRS_POWER8		MMU_FTRS_POWER6
#define MMU_FTRS_POWER9		MMU_FTRS_POWER6
#define MMU_FTRS_POWER10	MMU_FTRS_POWER6
#define MMU_FTRS_CELL		MMU_FTRS_DEFAULT_HPTE_ARCH_V2 | \
				MMU_FTR_CI_LARGE_PAGE
#define MMU_FTRS_PA6T		MMU_FTRS_DEFAULT_HPTE_ARCH_V2 | \
				MMU_FTR_CI_LARGE_PAGE | MMU_FTR_NO_SLBIE_B
#ifndef __ASSEMBLY__
#include <linux/bug.h>
#include <asm/cputable.h>
#include <asm/page.h>
typedef pte_t *pgtable_t;
enum {
	MMU_FTRS_POSSIBLE =
#if defined(CONFIG_PPC_BOOK3S_604)
		MMU_FTR_HPTE_TABLE |
#endif
#ifdef CONFIG_PPC_8xx
		MMU_FTR_TYPE_8xx |
#endif
#ifdef CONFIG_40x
		MMU_FTR_TYPE_40x |
#endif
#ifdef CONFIG_PPC_47x
		MMU_FTR_TYPE_47x | MMU_FTR_USE_TLBIVAX_BCAST | MMU_FTR_LOCK_BCAST_INVAL |
#elif defined(CONFIG_44x)
		MMU_FTR_TYPE_44x |
#endif
#ifdef CONFIG_PPC_E500
		MMU_FTR_TYPE_FSL_E | MMU_FTR_BIG_PHYS | MMU_FTR_USE_TLBILX |
#endif
#ifdef CONFIG_PPC_BOOK3S_32
		MMU_FTR_USE_HIGH_BATS |
#endif
#ifdef CONFIG_PPC_83xx
		MMU_FTR_NEED_DTLB_SW_LRU |
#endif
#ifdef CONFIG_PPC_BOOK3S_64
		MMU_FTR_KERNEL_RO |
#ifdef CONFIG_PPC_64S_HASH_MMU
		MMU_FTR_NO_SLBIE_B | MMU_FTR_16M_PAGE | MMU_FTR_TLBIEL |
		MMU_FTR_LOCKLESS_TLBIE | MMU_FTR_CI_LARGE_PAGE |
		MMU_FTR_1T_SEGMENT | MMU_FTR_TLBIE_CROP_VA |
		MMU_FTR_68_BIT_VA | MMU_FTR_HPTE_TABLE |
#endif
#ifdef CONFIG_PPC_RADIX_MMU
		MMU_FTR_TYPE_RADIX |
		MMU_FTR_GTSE | MMU_FTR_NX_DSI |
#endif  
#endif
#ifdef CONFIG_PPC_KUAP
	MMU_FTR_KUAP |
#endif  
#ifdef CONFIG_PPC_MEM_KEYS
	MMU_FTR_PKEY |
#endif
#ifdef CONFIG_PPC_KUEP
	MMU_FTR_BOOK3S_KUEP |
#endif  
		0,
};
#if defined(CONFIG_PPC_BOOK3S_604) && !defined(CONFIG_PPC_BOOK3S_603)
#define MMU_FTRS_ALWAYS		MMU_FTR_HPTE_TABLE
#endif
#ifdef CONFIG_PPC_8xx
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_8xx
#endif
#ifdef CONFIG_40x
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_40x
#endif
#ifdef CONFIG_PPC_47x
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_47x
#elif defined(CONFIG_44x)
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_44x
#endif
#ifdef CONFIG_PPC_E500
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_FSL_E
#endif
#if defined(CONFIG_PPC_RADIX_MMU) && !defined(CONFIG_PPC_64S_HASH_MMU)
#define MMU_FTRS_ALWAYS		MMU_FTR_TYPE_RADIX
#elif !defined(CONFIG_PPC_RADIX_MMU) && defined(CONFIG_PPC_64S_HASH_MMU)
#define MMU_FTRS_ALWAYS		MMU_FTR_HPTE_TABLE
#endif
#ifndef MMU_FTRS_ALWAYS
#define MMU_FTRS_ALWAYS		0
#endif
static __always_inline bool early_mmu_has_feature(unsigned long feature)
{
	if (MMU_FTRS_ALWAYS & feature)
		return true;
	return !!(MMU_FTRS_POSSIBLE & cur_cpu_spec->mmu_features & feature);
}
#ifdef CONFIG_JUMP_LABEL_FEATURE_CHECKS
#include <linux/jump_label.h>
#define NUM_MMU_FTR_KEYS	32
extern struct static_key_true mmu_feature_keys[NUM_MMU_FTR_KEYS];
extern void mmu_feature_keys_init(void);
static __always_inline bool mmu_has_feature(unsigned long feature)
{
	int i;
#ifndef __clang__  
	BUILD_BUG_ON(!__builtin_constant_p(feature));
#endif
#ifdef CONFIG_JUMP_LABEL_FEATURE_CHECK_DEBUG
	if (!static_key_initialized) {
		printk("Warning! mmu_has_feature() used prior to jump label init!\n");
		dump_stack();
		return early_mmu_has_feature(feature);
	}
#endif
	if (MMU_FTRS_ALWAYS & feature)
		return true;
	if (!(MMU_FTRS_POSSIBLE & feature))
		return false;
	i = __builtin_ctzl(feature);
	return static_branch_likely(&mmu_feature_keys[i]);
}
static inline void mmu_clear_feature(unsigned long feature)
{
	int i;
	i = __builtin_ctzl(feature);
	cur_cpu_spec->mmu_features &= ~feature;
	static_branch_disable(&mmu_feature_keys[i]);
}
#else
static inline void mmu_feature_keys_init(void)
{
}
static __always_inline bool mmu_has_feature(unsigned long feature)
{
	return early_mmu_has_feature(feature);
}
static inline void mmu_clear_feature(unsigned long feature)
{
	cur_cpu_spec->mmu_features &= ~feature;
}
#endif  
extern unsigned int __start___mmu_ftr_fixup, __stop___mmu_ftr_fixup;
#ifdef CONFIG_PPC64
extern u64 ppc64_rma_size;
extern void mmu_cleanup_all(void);
extern void radix__mmu_cleanup_all(void);
extern void mmu_partition_table_init(void);
extern void mmu_partition_table_set_entry(unsigned int lpid, unsigned long dw0,
					  unsigned long dw1, bool flush);
#endif  
struct mm_struct;
#ifdef CONFIG_DEBUG_VM
extern void assert_pte_locked(struct mm_struct *mm, unsigned long addr);
#else  
static inline void assert_pte_locked(struct mm_struct *mm, unsigned long addr)
{
}
#endif  
static __always_inline bool radix_enabled(void)
{
	return mmu_has_feature(MMU_FTR_TYPE_RADIX);
}
static __always_inline bool early_radix_enabled(void)
{
	return early_mmu_has_feature(MMU_FTR_TYPE_RADIX);
}
#ifdef CONFIG_STRICT_KERNEL_RWX
static inline bool strict_kernel_rwx_enabled(void)
{
	return rodata_enabled;
}
#else
static inline bool strict_kernel_rwx_enabled(void)
{
	return false;
}
#endif
static inline bool strict_module_rwx_enabled(void)
{
	return IS_ENABLED(CONFIG_STRICT_MODULE_RWX) && strict_kernel_rwx_enabled();
}
#endif  
#define MMU_PAGE_4K	0
#define MMU_PAGE_16K	1
#define MMU_PAGE_64K	2
#define MMU_PAGE_64K_AP	3	 
#define MMU_PAGE_256K	4
#define MMU_PAGE_512K	5
#define MMU_PAGE_1M	6
#define MMU_PAGE_2M	7
#define MMU_PAGE_4M	8
#define MMU_PAGE_8M	9
#define MMU_PAGE_16M	10
#define MMU_PAGE_64M	11
#define MMU_PAGE_256M	12
#define MMU_PAGE_1G	13
#define MMU_PAGE_16G	14
#define MMU_PAGE_64G	15
#define MMU_PAGE_COUNT	16
#ifdef CONFIG_PPC_BOOK3S_64
#include <asm/book3s/64/mmu.h>
#else  
#ifndef __ASSEMBLY__
extern void early_init_mmu(void);
extern void early_init_mmu_secondary(void);
extern void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				       phys_addr_t first_memblock_size);
static inline void mmu_early_init_devtree(void) { }
static inline void pkey_early_init_devtree(void) {}
extern void *abatron_pteptrs[2];
#endif  
#endif
#if defined(CONFIG_PPC_BOOK3S_32)
#include <asm/book3s/32/mmu-hash.h>
#elif defined(CONFIG_PPC_MMU_NOHASH)
#include <asm/nohash/mmu.h>
#endif
#endif  
#endif  
