#ifndef _ASM_POWERPC_BOOK3S_64_MMU_HASH_H_
#define _ASM_POWERPC_BOOK3S_64_MMU_HASH_H_
#include <asm/page.h>
#include <asm/bug.h>
#include <asm/asm-const.h>
#include <asm/book3s/64/pgtable.h>
#include <asm/book3s/64/slice.h>
#include <asm/task_size_64.h>
#include <asm/cpu_has_feature.h>
#define SLB_NUM_BOLTED		2
#define SLB_CACHE_ENTRIES	8
#define SLB_MIN_SIZE		32
#define SLB_ESID_V		ASM_CONST(0x0000000008000000)  
#define SLB_VSID_SHIFT		12
#define SLB_VSID_SHIFT_256M	SLB_VSID_SHIFT
#define SLB_VSID_SHIFT_1T	24
#define SLB_VSID_SSIZE_SHIFT	62
#define SLB_VSID_B		ASM_CONST(0xc000000000000000)
#define SLB_VSID_B_256M		ASM_CONST(0x0000000000000000)
#define SLB_VSID_B_1T		ASM_CONST(0x4000000000000000)
#define SLB_VSID_KS		ASM_CONST(0x0000000000000800)
#define SLB_VSID_KP		ASM_CONST(0x0000000000000400)
#define SLB_VSID_N		ASM_CONST(0x0000000000000200)  
#define SLB_VSID_L		ASM_CONST(0x0000000000000100)
#define SLB_VSID_C		ASM_CONST(0x0000000000000080)  
#define SLB_VSID_LP		ASM_CONST(0x0000000000000030)
#define SLB_VSID_LP_00		ASM_CONST(0x0000000000000000)
#define SLB_VSID_LP_01		ASM_CONST(0x0000000000000010)
#define SLB_VSID_LP_10		ASM_CONST(0x0000000000000020)
#define SLB_VSID_LP_11		ASM_CONST(0x0000000000000030)
#define SLB_VSID_LLP		(SLB_VSID_L|SLB_VSID_LP)
#define SLB_VSID_KERNEL		(SLB_VSID_KP)
#define SLB_VSID_USER		(SLB_VSID_KP|SLB_VSID_KS|SLB_VSID_C)
#define SLBIE_C			(0x08000000)
#define SLBIE_SSIZE_SHIFT	25
#define HPTES_PER_GROUP 8
#define HPTE_V_SSIZE_SHIFT	62
#define HPTE_V_AVPN_SHIFT	7
#define HPTE_V_COMMON_BITS	ASM_CONST(0x000fffffffffffff)
#define HPTE_V_AVPN		ASM_CONST(0x3fffffffffffff80)
#define HPTE_V_AVPN_3_0		ASM_CONST(0x000fffffffffff80)
#define HPTE_V_AVPN_VAL(x)	(((x) & HPTE_V_AVPN) >> HPTE_V_AVPN_SHIFT)
#define HPTE_V_COMPARE(x,y)	(!(((x) ^ (y)) & 0xffffffffffffff80UL))
#define HPTE_V_BOLTED		ASM_CONST(0x0000000000000010)
#define HPTE_V_LOCK		ASM_CONST(0x0000000000000008)
#define HPTE_V_LARGE		ASM_CONST(0x0000000000000004)
#define HPTE_V_SECONDARY	ASM_CONST(0x0000000000000002)
#define HPTE_V_VALID		ASM_CONST(0x0000000000000001)
#define HPTE_R_3_0_SSIZE_SHIFT	58
#define HPTE_R_3_0_SSIZE_MASK	(3ull << HPTE_R_3_0_SSIZE_SHIFT)
#define HPTE_R_PP0		ASM_CONST(0x8000000000000000)
#define HPTE_R_TS		ASM_CONST(0x4000000000000000)
#define HPTE_R_KEY_HI		ASM_CONST(0x3000000000000000)
#define HPTE_R_KEY_BIT4		ASM_CONST(0x2000000000000000)
#define HPTE_R_KEY_BIT3		ASM_CONST(0x1000000000000000)
#define HPTE_R_RPN_SHIFT	12
#define HPTE_R_RPN		ASM_CONST(0x0ffffffffffff000)
#define HPTE_R_RPN_3_0		ASM_CONST(0x01fffffffffff000)
#define HPTE_R_PP		ASM_CONST(0x0000000000000003)
#define HPTE_R_PPP		ASM_CONST(0x8000000000000003)
#define HPTE_R_N		ASM_CONST(0x0000000000000004)
#define HPTE_R_G		ASM_CONST(0x0000000000000008)
#define HPTE_R_M		ASM_CONST(0x0000000000000010)
#define HPTE_R_I		ASM_CONST(0x0000000000000020)
#define HPTE_R_W		ASM_CONST(0x0000000000000040)
#define HPTE_R_WIMG		ASM_CONST(0x0000000000000078)
#define HPTE_R_C		ASM_CONST(0x0000000000000080)
#define HPTE_R_R		ASM_CONST(0x0000000000000100)
#define HPTE_R_KEY_LO		ASM_CONST(0x0000000000000e00)
#define HPTE_R_KEY_BIT2		ASM_CONST(0x0000000000000800)
#define HPTE_R_KEY_BIT1		ASM_CONST(0x0000000000000400)
#define HPTE_R_KEY_BIT0		ASM_CONST(0x0000000000000200)
#define HPTE_R_KEY		(HPTE_R_KEY_LO | HPTE_R_KEY_HI)
#define HPTE_V_1TB_SEG		ASM_CONST(0x4000000000000000)
#define HPTE_V_VRMA_MASK	ASM_CONST(0x4001ffffff000000)
#define PP_RWXX	0	 
#define PP_RWRX 1	 
#define PP_RWRW 2	 
#define PP_RXRX 3	 
#define PP_RXXX	(HPTE_R_PP0 | 2)	 
#define TLBIEL_INVAL_SEL_MASK	0xc00	 
#define  TLBIEL_INVAL_PAGE	0x000	 
#define  TLBIEL_INVAL_SET_LPID	0x800	 
#define  TLBIEL_INVAL_SET	0xc00	 
#define TLBIEL_INVAL_SET_MASK	0xfff000	 
#define TLBIEL_INVAL_SET_SHIFT	12
#define POWER7_TLB_SETS		128	 
#define POWER8_TLB_SETS		512	 
#define POWER9_TLB_SETS_HASH	256	 
#define POWER9_TLB_SETS_RADIX	128	 
#ifndef __ASSEMBLY__
struct mmu_hash_ops {
	void            (*hpte_invalidate)(unsigned long slot,
					   unsigned long vpn,
					   int bpsize, int apsize,
					   int ssize, int local);
	long		(*hpte_updatepp)(unsigned long slot,
					 unsigned long newpp,
					 unsigned long vpn,
					 int bpsize, int apsize,
					 int ssize, unsigned long flags);
	void            (*hpte_updateboltedpp)(unsigned long newpp,
					       unsigned long ea,
					       int psize, int ssize);
	long		(*hpte_insert)(unsigned long hpte_group,
				       unsigned long vpn,
				       unsigned long prpn,
				       unsigned long rflags,
				       unsigned long vflags,
				       int psize, int apsize,
				       int ssize);
	long		(*hpte_remove)(unsigned long hpte_group);
	int             (*hpte_removebolted)(unsigned long ea,
					     int psize, int ssize);
	void		(*flush_hash_range)(unsigned long number, int local);
	void		(*hugepage_invalidate)(unsigned long vsid,
					       unsigned long addr,
					       unsigned char *hpte_slot_array,
					       int psize, int ssize, int local);
	int		(*resize_hpt)(unsigned long shift);
	void		(*hpte_clear_all)(void);
};
extern struct mmu_hash_ops mmu_hash_ops;
struct hash_pte {
	__be64 v;
	__be64 r;
};
extern struct hash_pte *htab_address;
extern unsigned long htab_size_bytes;
extern unsigned long htab_hash_mask;
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
static inline unsigned int ap_to_shift(unsigned long ap)
{
	int psize;
	for (psize = 0; psize < MMU_PAGE_COUNT; psize++) {
		if (mmu_psize_defs[psize].ap == ap)
			return mmu_psize_defs[psize].shift;
	}
	return -1;
}
static inline unsigned long get_sllp_encoding(int psize)
{
	unsigned long sllp;
	sllp = ((mmu_psize_defs[psize].sllp & SLB_VSID_L) >> 6) |
		((mmu_psize_defs[psize].sllp & SLB_VSID_LP) >> 4);
	return sllp;
}
#endif  
#define MMU_SEGSIZE_256M	0
#define MMU_SEGSIZE_1T		1
#define VPN_SHIFT	12
#define LP_SHIFT	12
#define LP_BITS		8
#define LP_MASK(i)	((0xFF >> (i)) << LP_SHIFT)
#ifndef __ASSEMBLY__
static inline int slb_vsid_shift(int ssize)
{
	if (ssize == MMU_SEGSIZE_256M)
		return SLB_VSID_SHIFT;
	return SLB_VSID_SHIFT_1T;
}
static inline int segment_shift(int ssize)
{
	if (ssize == MMU_SEGSIZE_256M)
		return SID_SHIFT;
	return SID_SHIFT_1T;
}
extern u8 hpte_page_sizes[1 << LP_BITS];
static inline unsigned long __hpte_page_size(unsigned long h, unsigned long l,
					     bool is_base_size)
{
	unsigned int i, lp;
	if (!(h & HPTE_V_LARGE))
		return 1ul << 12;
	lp = (l >> LP_SHIFT) & ((1 << LP_BITS) - 1);
	i = hpte_page_sizes[lp];
	if (!i)
		return 0;
	if (!is_base_size)
		i >>= 4;
	return 1ul << mmu_psize_defs[i & 0xf].shift;
}
static inline unsigned long hpte_page_size(unsigned long h, unsigned long l)
{
	return __hpte_page_size(h, l, 0);
}
static inline unsigned long hpte_base_page_size(unsigned long h, unsigned long l)
{
	return __hpte_page_size(h, l, 1);
}
extern int mmu_kernel_ssize;
extern int mmu_highuser_ssize;
extern u16 mmu_slb_size;
extern unsigned long tce_alloc_start, tce_alloc_end;
extern int mmu_ci_restrictions;
static inline unsigned long hpte_encode_avpn(unsigned long vpn, int psize,
					     int ssize)
{
	unsigned long v;
	v = (vpn >> (23 - VPN_SHIFT)) & ~(mmu_psize_defs[psize].avpnm);
	v <<= HPTE_V_AVPN_SHIFT;
	v |= ((unsigned long) ssize) << HPTE_V_SSIZE_SHIFT;
	return v;
}
static inline unsigned long hpte_old_to_new_v(unsigned long v)
{
	return v & HPTE_V_COMMON_BITS;
}
static inline unsigned long hpte_old_to_new_r(unsigned long v, unsigned long r)
{
	return (r & ~HPTE_R_3_0_SSIZE_MASK) |
		(((v) >> HPTE_V_SSIZE_SHIFT) << HPTE_R_3_0_SSIZE_SHIFT);
}
static inline unsigned long hpte_new_to_old_v(unsigned long v, unsigned long r)
{
	return (v & HPTE_V_COMMON_BITS) |
		((r & HPTE_R_3_0_SSIZE_MASK) <<
		 (HPTE_V_SSIZE_SHIFT - HPTE_R_3_0_SSIZE_SHIFT));
}
static inline unsigned long hpte_new_to_old_r(unsigned long r)
{
	return r & ~HPTE_R_3_0_SSIZE_MASK;
}
static inline unsigned long hpte_get_old_v(struct hash_pte *hptep)
{
	unsigned long hpte_v;
	hpte_v = be64_to_cpu(hptep->v);
	if (cpu_has_feature(CPU_FTR_ARCH_300))
		hpte_v = hpte_new_to_old_v(hpte_v, be64_to_cpu(hptep->r));
	return hpte_v;
}
static inline unsigned long hpte_encode_v(unsigned long vpn, int base_psize,
					  int actual_psize, int ssize)
{
	unsigned long v;
	v = hpte_encode_avpn(vpn, base_psize, ssize);
	if (actual_psize != MMU_PAGE_4K)
		v |= HPTE_V_LARGE;
	return v;
}
static inline unsigned long hpte_encode_r(unsigned long pa, int base_psize,
					  int actual_psize)
{
	if (actual_psize == MMU_PAGE_4K)
		return pa & HPTE_R_RPN;
	else {
		unsigned int penc = mmu_psize_defs[base_psize].penc[actual_psize];
		unsigned int shift = mmu_psize_defs[actual_psize].shift;
		return (pa & ~((1ul << shift) - 1)) | (penc << LP_SHIFT);
	}
}
static inline unsigned long hpt_vpn(unsigned long ea,
				    unsigned long vsid, int ssize)
{
	unsigned long mask;
	int s_shift = segment_shift(ssize);
	mask = (1ul << (s_shift - VPN_SHIFT)) - 1;
	return (vsid << (s_shift - VPN_SHIFT)) | ((ea >> VPN_SHIFT) & mask);
}
static inline unsigned long hpt_hash(unsigned long vpn,
				     unsigned int shift, int ssize)
{
	unsigned long mask;
	unsigned long hash, vsid;
	if (ssize == MMU_SEGSIZE_256M) {
		mask = (1ul << (SID_SHIFT - VPN_SHIFT)) - 1;
		hash = (vpn >> (SID_SHIFT - VPN_SHIFT)) ^
			((vpn & mask) >> (shift - VPN_SHIFT));
	} else {
		mask = (1ul << (SID_SHIFT_1T - VPN_SHIFT)) - 1;
		vsid = vpn >> (SID_SHIFT_1T - VPN_SHIFT);
		hash = vsid ^ (vsid << 25) ^
			((vpn & mask) >> (shift - VPN_SHIFT)) ;
	}
	return hash & 0x7fffffffffUL;
}
#define HPTE_LOCAL_UPDATE	0x1
#define HPTE_NOHPTE_UPDATE	0x2
#define HPTE_USE_KERNEL_KEY	0x4
long hpte_insert_repeating(unsigned long hash, unsigned long vpn, unsigned long pa,
			   unsigned long rlags, unsigned long vflags, int psize, int ssize);
extern int __hash_page_4K(unsigned long ea, unsigned long access,
			  unsigned long vsid, pte_t *ptep, unsigned long trap,
			  unsigned long flags, int ssize, int subpage_prot);
extern int __hash_page_64K(unsigned long ea, unsigned long access,
			   unsigned long vsid, pte_t *ptep, unsigned long trap,
			   unsigned long flags, int ssize);
struct mm_struct;
unsigned int hash_page_do_lazy_icache(unsigned int pp, pte_t pte, int trap);
extern int hash_page_mm(struct mm_struct *mm, unsigned long ea,
			unsigned long access, unsigned long trap,
			unsigned long flags);
extern int hash_page(unsigned long ea, unsigned long access, unsigned long trap,
		     unsigned long dsisr);
void low_hash_fault(struct pt_regs *regs, unsigned long address, int rc);
int __hash_page(unsigned long trap, unsigned long ea, unsigned long dsisr, unsigned long msr);
int __hash_page_huge(unsigned long ea, unsigned long access, unsigned long vsid,
		     pte_t *ptep, unsigned long trap, unsigned long flags,
		     int ssize, unsigned int shift, unsigned int mmu_psize);
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
extern int __hash_page_thp(unsigned long ea, unsigned long access,
			   unsigned long vsid, pmd_t *pmdp, unsigned long trap,
			   unsigned long flags, int ssize, unsigned int psize);
#else
static inline int __hash_page_thp(unsigned long ea, unsigned long access,
				  unsigned long vsid, pmd_t *pmdp,
				  unsigned long trap, unsigned long flags,
				  int ssize, unsigned int psize)
{
	BUG();
	return -1;
}
#endif
extern void hash_failure_debug(unsigned long ea, unsigned long access,
			       unsigned long vsid, unsigned long trap,
			       int ssize, int psize, int lpsize,
			       unsigned long pte);
extern int htab_bolt_mapping(unsigned long vstart, unsigned long vend,
			     unsigned long pstart, unsigned long prot,
			     int psize, int ssize);
int htab_remove_mapping(unsigned long vstart, unsigned long vend,
			int psize, int ssize);
extern void pseries_add_gpage(u64 addr, u64 page_size, unsigned long number_of_pages);
extern void demote_segment_4k(struct mm_struct *mm, unsigned long addr);
extern void hash__setup_new_exec(void);
#ifdef CONFIG_PPC_PSERIES
void hpte_init_pseries(void);
#else
static inline void hpte_init_pseries(void) { }
#endif
extern void hpte_init_native(void);
struct slb_entry {
	u64	esid;
	u64	vsid;
};
extern void slb_initialize(void);
void slb_flush_and_restore_bolted(void);
void slb_flush_all_realmode(void);
void __slb_restore_bolted_realmode(void);
void slb_restore_bolted_realmode(void);
void slb_save_contents(struct slb_entry *slb_ptr);
void slb_dump_contents(struct slb_entry *slb_ptr);
extern void slb_vmalloc_update(void);
void preload_new_slb_context(unsigned long start, unsigned long sp);
#ifdef CONFIG_PPC_64S_HASH_MMU
void slb_set_size(u16 size);
#else
static inline void slb_set_size(u16 size) { }
#endif
#endif  
#define VA_BITS			68
#define CONTEXT_BITS		19
#define ESID_BITS		(VA_BITS - (SID_SHIFT + CONTEXT_BITS))
#define ESID_BITS_1T		(VA_BITS - (SID_SHIFT_1T + CONTEXT_BITS))
#define ESID_BITS_MASK		((1 << ESID_BITS) - 1)
#define ESID_BITS_1T_MASK	((1 << ESID_BITS_1T) - 1)
#if (H_MAX_PHYSMEM_BITS > MAX_EA_BITS_PER_CONTEXT)
#define MAX_KERNEL_CTX_CNT	(1UL << (H_MAX_PHYSMEM_BITS - MAX_EA_BITS_PER_CONTEXT))
#else
#define MAX_KERNEL_CTX_CNT	1
#endif
#define MAX_VMALLOC_CTX_CNT	1
#define MAX_IO_CTX_CNT		1
#define MAX_VMEMMAP_CTX_CNT	1
#define MAX_USER_CONTEXT	((ASM_CONST(1) << CONTEXT_BITS) - 2)
#define MIN_USER_CONTEXT	(MAX_KERNEL_CTX_CNT + MAX_VMALLOC_CTX_CNT + \
				 MAX_IO_CTX_CNT + MAX_VMEMMAP_CTX_CNT + 2)
#define MAX_USER_CONTEXT_65BIT_VA ((ASM_CONST(1) << (65 - (SID_SHIFT + ESID_BITS))) - 2)
#define VSID_MULTIPLIER_256M	ASM_CONST(12538073)	 
#define VSID_BITS_256M		(VA_BITS - SID_SHIFT)
#define VSID_BITS_65_256M	(65 - SID_SHIFT)
#define VSID_MULINV_256M	ASM_CONST(665548017062)
#define VSID_MULTIPLIER_1T	ASM_CONST(12538073)	 
#define VSID_BITS_1T		(VA_BITS - SID_SHIFT_1T)
#define VSID_BITS_65_1T		(65 - SID_SHIFT_1T)
#define VSID_MULINV_1T		ASM_CONST(209034062)
#define VRMA_VSID	0x1ffffffUL
#define USER_VSID_RANGE	(1UL << (ESID_BITS + SID_SHIFT))
#define SLICE_ARRAY_SIZE	(H_PGTABLE_RANGE >> 41)
#define LOW_SLICE_ARRAY_SZ	(BITS_PER_LONG / BITS_PER_BYTE)
#define TASK_SLICE_ARRAY_SZ(x)	((x)->hash_context->slb_addr_limit >> 41)
#ifndef __ASSEMBLY__
#ifdef CONFIG_PPC_SUBPAGE_PROT
struct subpage_prot_table {
	unsigned long maxaddr;	 
	unsigned int **protptrs[(TASK_SIZE_USER64 >> 43)];
	unsigned int *low_prot[4];
};
#define SBP_L1_BITS		(PAGE_SHIFT - 2)
#define SBP_L2_BITS		(PAGE_SHIFT - 3)
#define SBP_L1_COUNT		(1 << SBP_L1_BITS)
#define SBP_L2_COUNT		(1 << SBP_L2_BITS)
#define SBP_L2_SHIFT		(PAGE_SHIFT + SBP_L1_BITS)
#define SBP_L3_SHIFT		(SBP_L2_SHIFT + SBP_L2_BITS)
extern void subpage_prot_free(struct mm_struct *mm);
#else
static inline void subpage_prot_free(struct mm_struct *mm) {}
#endif  
struct slice_mask {
	u64 low_slices;
	DECLARE_BITMAP(high_slices, SLICE_NUM_HIGH);
};
struct hash_mm_context {
	u16 user_psize;  
	unsigned char low_slices_psize[LOW_SLICE_ARRAY_SZ];
	unsigned char high_slices_psize[SLICE_ARRAY_SIZE];
	unsigned long slb_addr_limit;
#ifdef CONFIG_PPC_64K_PAGES
	struct slice_mask mask_64k;
#endif
	struct slice_mask mask_4k;
#ifdef CONFIG_HUGETLB_PAGE
	struct slice_mask mask_16m;
	struct slice_mask mask_16g;
#endif
#ifdef CONFIG_PPC_SUBPAGE_PROT
	struct subpage_prot_table *spt;
#endif  
};
#if 0
#define vsid_scramble(protovsid, size) \
	((((protovsid) * VSID_MULTIPLIER_##size) % VSID_MODULUS_##size))
#define vsid_scramble(protovsid, size) \
	({								 \
		unsigned long x;					 \
		x = (protovsid) * VSID_MULTIPLIER_##size;		 \
		x = (x >> VSID_BITS_##size) + (x & VSID_MODULUS_##size); \
		(x + ((x+1) >> VSID_BITS_##size)) & VSID_MODULUS_##size; \
	})
#else  
static inline unsigned long vsid_scramble(unsigned long protovsid,
				  unsigned long vsid_multiplier, int vsid_bits)
{
	unsigned long vsid;
	unsigned long vsid_modulus = ((1UL << vsid_bits) - 1);
	vsid = protovsid * vsid_multiplier;
	vsid = (vsid >> vsid_bits) + (vsid & vsid_modulus);
	return (vsid + ((vsid + 1) >> vsid_bits)) & vsid_modulus;
}
#endif  
static inline int user_segment_size(unsigned long addr)
{
	if (addr >= (1UL << SID_SHIFT_1T))
		return mmu_highuser_ssize;
	return MMU_SEGSIZE_256M;
}
static inline unsigned long get_vsid(unsigned long context, unsigned long ea,
				     int ssize)
{
	unsigned long va_bits = VA_BITS;
	unsigned long vsid_bits;
	unsigned long protovsid;
	if ((ea & EA_MASK)  >= H_PGTABLE_RANGE)
		return 0;
	if (!mmu_has_feature(MMU_FTR_68_BIT_VA))
		va_bits = 65;
	if (ssize == MMU_SEGSIZE_256M) {
		vsid_bits = va_bits - SID_SHIFT;
		protovsid = (context << ESID_BITS) |
			((ea >> SID_SHIFT) & ESID_BITS_MASK);
		return vsid_scramble(protovsid, VSID_MULTIPLIER_256M, vsid_bits);
	}
	vsid_bits = va_bits - SID_SHIFT_1T;
	protovsid = (context << ESID_BITS_1T) |
		((ea >> SID_SHIFT_1T) & ESID_BITS_1T_MASK);
	return vsid_scramble(protovsid, VSID_MULTIPLIER_1T, vsid_bits);
}
static inline unsigned long get_kernel_context(unsigned long ea)
{
	unsigned long region_id = get_region_id(ea);
	unsigned long ctx;
	if (region_id == LINEAR_MAP_REGION_ID) {
		ctx =  1 + ((ea & EA_MASK) >> MAX_EA_BITS_PER_CONTEXT);
	} else
		ctx = region_id + MAX_KERNEL_CTX_CNT - 1;
	return ctx;
}
static inline unsigned long get_kernel_vsid(unsigned long ea, int ssize)
{
	unsigned long context;
	if (!is_kernel_addr(ea))
		return 0;
	context = get_kernel_context(ea);
	return get_vsid(context, ea, ssize);
}
unsigned htab_shift_for_mem_size(unsigned long mem_size);
enum slb_index {
	LINEAR_INDEX	= 0,  
	KSTACK_INDEX	= 1,  
};
#define slb_esid_mask(ssize)	\
	(((ssize) == MMU_SEGSIZE_256M) ? ESID_MASK : ESID_MASK_1T)
static inline unsigned long mk_esid_data(unsigned long ea, int ssize,
					 enum slb_index index)
{
	return (ea & slb_esid_mask(ssize)) | SLB_ESID_V | index;
}
static inline unsigned long __mk_vsid_data(unsigned long vsid, int ssize,
					   unsigned long flags)
{
	return (vsid << slb_vsid_shift(ssize)) | flags |
		((unsigned long)ssize << SLB_VSID_SSIZE_SHIFT);
}
static inline unsigned long mk_vsid_data(unsigned long ea, int ssize,
					 unsigned long flags)
{
	return __mk_vsid_data(get_kernel_vsid(ea, ssize), ssize, flags);
}
#endif  
#endif  
