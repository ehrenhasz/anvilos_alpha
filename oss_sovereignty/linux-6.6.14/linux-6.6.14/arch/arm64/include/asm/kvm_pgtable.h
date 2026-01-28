#ifndef __ARM64_KVM_PGTABLE_H__
#define __ARM64_KVM_PGTABLE_H__
#include <linux/bits.h>
#include <linux/kvm_host.h>
#include <linux/types.h>
#define KVM_PGTABLE_MAX_LEVELS		4U
#ifdef CONFIG_ARM64_4K_PAGES
#define KVM_PGTABLE_MIN_BLOCK_LEVEL	1U
#else
#define KVM_PGTABLE_MIN_BLOCK_LEVEL	2U
#endif
static inline u64 kvm_get_parange(u64 mmfr0)
{
	u64 parange = cpuid_feature_extract_unsigned_field(mmfr0,
				ID_AA64MMFR0_EL1_PARANGE_SHIFT);
	if (parange > ID_AA64MMFR0_EL1_PARANGE_MAX)
		parange = ID_AA64MMFR0_EL1_PARANGE_MAX;
	return parange;
}
typedef u64 kvm_pte_t;
#define KVM_PTE_VALID			BIT(0)
#define KVM_PTE_ADDR_MASK		GENMASK(47, PAGE_SHIFT)
#define KVM_PTE_ADDR_51_48		GENMASK(15, 12)
#define KVM_PHYS_INVALID		(-1ULL)
static inline bool kvm_pte_valid(kvm_pte_t pte)
{
	return pte & KVM_PTE_VALID;
}
static inline u64 kvm_pte_to_phys(kvm_pte_t pte)
{
	u64 pa = pte & KVM_PTE_ADDR_MASK;
	if (PAGE_SHIFT == 16)
		pa |= FIELD_GET(KVM_PTE_ADDR_51_48, pte) << 48;
	return pa;
}
static inline kvm_pte_t kvm_phys_to_pte(u64 pa)
{
	kvm_pte_t pte = pa & KVM_PTE_ADDR_MASK;
	if (PAGE_SHIFT == 16) {
		pa &= GENMASK(51, 48);
		pte |= FIELD_PREP(KVM_PTE_ADDR_51_48, pa >> 48);
	}
	return pte;
}
static inline kvm_pfn_t kvm_pte_to_pfn(kvm_pte_t pte)
{
	return __phys_to_pfn(kvm_pte_to_phys(pte));
}
static inline u64 kvm_granule_shift(u32 level)
{
	return ARM64_HW_PGTABLE_LEVEL_SHIFT(level);
}
static inline u64 kvm_granule_size(u32 level)
{
	return BIT(kvm_granule_shift(level));
}
static inline bool kvm_level_supports_block_mapping(u32 level)
{
	return level >= KVM_PGTABLE_MIN_BLOCK_LEVEL;
}
static inline u32 kvm_supported_block_sizes(void)
{
	u32 level = KVM_PGTABLE_MIN_BLOCK_LEVEL;
	u32 r = 0;
	for (; level < KVM_PGTABLE_MAX_LEVELS; level++)
		r |= BIT(kvm_granule_shift(level));
	return r;
}
static inline bool kvm_is_block_size_supported(u64 size)
{
	bool is_power_of_two = IS_ALIGNED(size, size);
	return is_power_of_two && (size & kvm_supported_block_sizes());
}
struct kvm_pgtable_mm_ops {
	void*		(*zalloc_page)(void *arg);
	void*		(*zalloc_pages_exact)(size_t size);
	void		(*free_pages_exact)(void *addr, size_t size);
	void		(*free_unlinked_table)(void *addr, u32 level);
	void		(*get_page)(void *addr);
	void		(*put_page)(void *addr);
	int		(*page_count)(void *addr);
	void*		(*phys_to_virt)(phys_addr_t phys);
	phys_addr_t	(*virt_to_phys)(void *addr);
	void		(*dcache_clean_inval_poc)(void *addr, size_t size);
	void		(*icache_inval_pou)(void *addr, size_t size);
};
enum kvm_pgtable_stage2_flags {
	KVM_PGTABLE_S2_NOFWB			= BIT(0),
	KVM_PGTABLE_S2_IDMAP			= BIT(1),
};
enum kvm_pgtable_prot {
	KVM_PGTABLE_PROT_X			= BIT(0),
	KVM_PGTABLE_PROT_W			= BIT(1),
	KVM_PGTABLE_PROT_R			= BIT(2),
	KVM_PGTABLE_PROT_DEVICE			= BIT(3),
	KVM_PGTABLE_PROT_SW0			= BIT(55),
	KVM_PGTABLE_PROT_SW1			= BIT(56),
	KVM_PGTABLE_PROT_SW2			= BIT(57),
	KVM_PGTABLE_PROT_SW3			= BIT(58),
};
#define KVM_PGTABLE_PROT_RW	(KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_W)
#define KVM_PGTABLE_PROT_RWX	(KVM_PGTABLE_PROT_RW | KVM_PGTABLE_PROT_X)
#define PKVM_HOST_MEM_PROT	KVM_PGTABLE_PROT_RWX
#define PKVM_HOST_MMIO_PROT	KVM_PGTABLE_PROT_RW
#define PAGE_HYP		KVM_PGTABLE_PROT_RW
#define PAGE_HYP_EXEC		(KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_X)
#define PAGE_HYP_RO		(KVM_PGTABLE_PROT_R)
#define PAGE_HYP_DEVICE		(PAGE_HYP | KVM_PGTABLE_PROT_DEVICE)
typedef bool (*kvm_pgtable_force_pte_cb_t)(u64 addr, u64 end,
					   enum kvm_pgtable_prot prot);
enum kvm_pgtable_walk_flags {
	KVM_PGTABLE_WALK_LEAF			= BIT(0),
	KVM_PGTABLE_WALK_TABLE_PRE		= BIT(1),
	KVM_PGTABLE_WALK_TABLE_POST		= BIT(2),
	KVM_PGTABLE_WALK_SHARED			= BIT(3),
	KVM_PGTABLE_WALK_HANDLE_FAULT		= BIT(4),
	KVM_PGTABLE_WALK_SKIP_BBM_TLBI		= BIT(5),
	KVM_PGTABLE_WALK_SKIP_CMO		= BIT(6),
};
struct kvm_pgtable_visit_ctx {
	kvm_pte_t				*ptep;
	kvm_pte_t				old;
	void					*arg;
	struct kvm_pgtable_mm_ops		*mm_ops;
	u64					start;
	u64					addr;
	u64					end;
	u32					level;
	enum kvm_pgtable_walk_flags		flags;
};
typedef int (*kvm_pgtable_visitor_fn_t)(const struct kvm_pgtable_visit_ctx *ctx,
					enum kvm_pgtable_walk_flags visit);
static inline bool kvm_pgtable_walk_shared(const struct kvm_pgtable_visit_ctx *ctx)
{
	return ctx->flags & KVM_PGTABLE_WALK_SHARED;
}
struct kvm_pgtable_walker {
	const kvm_pgtable_visitor_fn_t		cb;
	void * const				arg;
	const enum kvm_pgtable_walk_flags	flags;
};
#if defined(__KVM_NVHE_HYPERVISOR__) || defined(__KVM_VHE_HYPERVISOR__)
typedef kvm_pte_t *kvm_pteref_t;
static inline kvm_pte_t *kvm_dereference_pteref(struct kvm_pgtable_walker *walker,
						kvm_pteref_t pteref)
{
	return pteref;
}
static inline int kvm_pgtable_walk_begin(struct kvm_pgtable_walker *walker)
{
	if (walker->flags & KVM_PGTABLE_WALK_SHARED)
		return -EPERM;
	return 0;
}
static inline void kvm_pgtable_walk_end(struct kvm_pgtable_walker *walker) {}
static inline bool kvm_pgtable_walk_lock_held(void)
{
	return true;
}
#else
typedef kvm_pte_t __rcu *kvm_pteref_t;
static inline kvm_pte_t *kvm_dereference_pteref(struct kvm_pgtable_walker *walker,
						kvm_pteref_t pteref)
{
	return rcu_dereference_check(pteref, !(walker->flags & KVM_PGTABLE_WALK_SHARED));
}
static inline int kvm_pgtable_walk_begin(struct kvm_pgtable_walker *walker)
{
	if (walker->flags & KVM_PGTABLE_WALK_SHARED)
		rcu_read_lock();
	return 0;
}
static inline void kvm_pgtable_walk_end(struct kvm_pgtable_walker *walker)
{
	if (walker->flags & KVM_PGTABLE_WALK_SHARED)
		rcu_read_unlock();
}
static inline bool kvm_pgtable_walk_lock_held(void)
{
	return rcu_read_lock_held();
}
#endif
struct kvm_pgtable {
	u32					ia_bits;
	u32					start_level;
	kvm_pteref_t				pgd;
	struct kvm_pgtable_mm_ops		*mm_ops;
	struct kvm_s2_mmu			*mmu;
	enum kvm_pgtable_stage2_flags		flags;
	kvm_pgtable_force_pte_cb_t		force_pte_cb;
};
int kvm_pgtable_hyp_init(struct kvm_pgtable *pgt, u32 va_bits,
			 struct kvm_pgtable_mm_ops *mm_ops);
void kvm_pgtable_hyp_destroy(struct kvm_pgtable *pgt);
int kvm_pgtable_hyp_map(struct kvm_pgtable *pgt, u64 addr, u64 size, u64 phys,
			enum kvm_pgtable_prot prot);
u64 kvm_pgtable_hyp_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size);
u64 kvm_get_vtcr(u64 mmfr0, u64 mmfr1, u32 phys_shift);
size_t kvm_pgtable_stage2_pgd_size(u64 vtcr);
int __kvm_pgtable_stage2_init(struct kvm_pgtable *pgt, struct kvm_s2_mmu *mmu,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      enum kvm_pgtable_stage2_flags flags,
			      kvm_pgtable_force_pte_cb_t force_pte_cb);
#define kvm_pgtable_stage2_init(pgt, mmu, mm_ops) \
	__kvm_pgtable_stage2_init(pgt, mmu, mm_ops, 0, NULL)
void kvm_pgtable_stage2_destroy(struct kvm_pgtable *pgt);
void kvm_pgtable_stage2_free_unlinked(struct kvm_pgtable_mm_ops *mm_ops, void *pgtable, u32 level);
kvm_pte_t *kvm_pgtable_stage2_create_unlinked(struct kvm_pgtable *pgt,
					      u64 phys, u32 level,
					      enum kvm_pgtable_prot prot,
					      void *mc, bool force_pte);
int kvm_pgtable_stage2_map(struct kvm_pgtable *pgt, u64 addr, u64 size,
			   u64 phys, enum kvm_pgtable_prot prot,
			   void *mc, enum kvm_pgtable_walk_flags flags);
int kvm_pgtable_stage2_set_owner(struct kvm_pgtable *pgt, u64 addr, u64 size,
				 void *mc, u8 owner_id);
int kvm_pgtable_stage2_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size);
int kvm_pgtable_stage2_wrprotect(struct kvm_pgtable *pgt, u64 addr, u64 size);
kvm_pte_t kvm_pgtable_stage2_mkyoung(struct kvm_pgtable *pgt, u64 addr);
bool kvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr,
					 u64 size, bool mkold);
int kvm_pgtable_stage2_relax_perms(struct kvm_pgtable *pgt, u64 addr,
				   enum kvm_pgtable_prot prot);
int kvm_pgtable_stage2_flush(struct kvm_pgtable *pgt, u64 addr, u64 size);
int kvm_pgtable_stage2_split(struct kvm_pgtable *pgt, u64 addr, u64 size,
			     struct kvm_mmu_memory_cache *mc);
int kvm_pgtable_walk(struct kvm_pgtable *pgt, u64 addr, u64 size,
		     struct kvm_pgtable_walker *walker);
int kvm_pgtable_get_leaf(struct kvm_pgtable *pgt, u64 addr,
			 kvm_pte_t *ptep, u32 *level);
enum kvm_pgtable_prot kvm_pgtable_stage2_pte_prot(kvm_pte_t pte);
enum kvm_pgtable_prot kvm_pgtable_hyp_pte_prot(kvm_pte_t pte);
void kvm_tlb_flush_vmid_range(struct kvm_s2_mmu *mmu,
				phys_addr_t addr, size_t size);
#endif	 
