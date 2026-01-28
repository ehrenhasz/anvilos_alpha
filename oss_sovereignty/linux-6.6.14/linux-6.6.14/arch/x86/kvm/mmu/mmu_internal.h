#ifndef __KVM_X86_MMU_INTERNAL_H
#define __KVM_X86_MMU_INTERNAL_H
#include <linux/types.h>
#include <linux/kvm_host.h>
#include <asm/kvm_host.h>
#ifdef CONFIG_KVM_PROVE_MMU
#define KVM_MMU_WARN_ON(x) WARN_ON_ONCE(x)
#else
#define KVM_MMU_WARN_ON(x) BUILD_BUG_ON_INVALID(x)
#endif
#define __PT_LEVEL_SHIFT(level, bits_per_level)	\
	(PAGE_SHIFT + ((level) - 1) * (bits_per_level))
#define __PT_INDEX(address, level, bits_per_level) \
	(((address) >> __PT_LEVEL_SHIFT(level, bits_per_level)) & ((1 << (bits_per_level)) - 1))
#define __PT_LVL_ADDR_MASK(base_addr_mask, level, bits_per_level) \
	((base_addr_mask) & ~((1ULL << (PAGE_SHIFT + (((level) - 1) * (bits_per_level)))) - 1))
#define __PT_LVL_OFFSET_MASK(base_addr_mask, level, bits_per_level) \
	((base_addr_mask) & ((1ULL << (PAGE_SHIFT + (((level) - 1) * (bits_per_level)))) - 1))
#define __PT_ENT_PER_PAGE(bits_per_level)  (1 << (bits_per_level))
#define INVALID_PAE_ROOT	0
#define IS_VALID_PAE_ROOT(x)	(!!(x))
static inline hpa_t kvm_mmu_get_dummy_root(void)
{
	return my_zero_pfn(0) << PAGE_SHIFT;
}
static inline bool kvm_mmu_is_dummy_root(hpa_t shadow_page)
{
	return is_zero_pfn(shadow_page >> PAGE_SHIFT);
}
typedef u64 __rcu *tdp_ptep_t;
struct kvm_mmu_page {
	struct list_head link;
	struct hlist_node hash_link;
	bool tdp_mmu_page;
	bool unsync;
	union {
		u8 mmu_valid_gen;
		bool tdp_mmu_scheduled_root_to_zap;
	};
	bool nx_huge_page_disallowed;
	union kvm_mmu_page_role role;
	gfn_t gfn;
	u64 *spt;
	u64 *shadowed_translation;
	union {
		int root_count;
		refcount_t tdp_mmu_root_count;
	};
	unsigned int unsync_children;
	union {
		struct kvm_rmap_head parent_ptes;  
		tdp_ptep_t ptep;
	};
	DECLARE_BITMAP(unsync_child_bitmap, 512);
	struct list_head possible_nx_huge_page_link;
#ifdef CONFIG_X86_32
	int clear_spte_count;
#endif
	atomic_t write_flooding_count;
#ifdef CONFIG_X86_64
	struct rcu_head rcu_head;
#endif
};
extern struct kmem_cache *mmu_page_header_cache;
static inline int kvm_mmu_role_as_id(union kvm_mmu_page_role role)
{
	return role.smm ? 1 : 0;
}
static inline int kvm_mmu_page_as_id(struct kvm_mmu_page *sp)
{
	return kvm_mmu_role_as_id(sp->role);
}
static inline bool kvm_mmu_page_ad_need_write_protect(struct kvm_mmu_page *sp)
{
	return kvm_x86_ops.cpu_dirty_log_size && sp->role.guest_mode;
}
static inline gfn_t gfn_round_for_level(gfn_t gfn, int level)
{
	return gfn & -KVM_PAGES_PER_HPAGE(level);
}
int mmu_try_to_unsync_pages(struct kvm *kvm, const struct kvm_memory_slot *slot,
			    gfn_t gfn, bool can_unsync, bool prefetch);
void kvm_mmu_gfn_disallow_lpage(const struct kvm_memory_slot *slot, gfn_t gfn);
void kvm_mmu_gfn_allow_lpage(const struct kvm_memory_slot *slot, gfn_t gfn);
bool kvm_mmu_slot_gfn_write_protect(struct kvm *kvm,
				    struct kvm_memory_slot *slot, u64 gfn,
				    int min_level);
static inline void kvm_flush_remote_tlbs_gfn(struct kvm *kvm, gfn_t gfn, int level)
{
	kvm_flush_remote_tlbs_range(kvm, gfn_round_for_level(gfn, level),
				    KVM_PAGES_PER_HPAGE(level));
}
unsigned int pte_list_count(struct kvm_rmap_head *rmap_head);
extern int nx_huge_pages;
static inline bool is_nx_huge_page_enabled(struct kvm *kvm)
{
	return READ_ONCE(nx_huge_pages) && !kvm->arch.disable_nx_huge_pages;
}
struct kvm_page_fault {
	const gpa_t addr;
	const u32 error_code;
	const bool prefetch;
	const bool exec;
	const bool write;
	const bool present;
	const bool rsvd;
	const bool user;
	const bool is_tdp;
	const bool nx_huge_page_workaround_enabled;
	bool huge_page_disallowed;
	u8 max_level;
	u8 req_level;
	u8 goal_level;
	gfn_t gfn;
	struct kvm_memory_slot *slot;
	unsigned long mmu_seq;
	kvm_pfn_t pfn;
	hva_t hva;
	bool map_writable;
	bool write_fault_to_shadow_pgtable;
};
int kvm_tdp_page_fault(struct kvm_vcpu *vcpu, struct kvm_page_fault *fault);
enum {
	RET_PF_CONTINUE = 0,
	RET_PF_RETRY,
	RET_PF_EMULATE,
	RET_PF_INVALID,
	RET_PF_FIXED,
	RET_PF_SPURIOUS,
};
static inline int kvm_mmu_do_page_fault(struct kvm_vcpu *vcpu, gpa_t cr2_or_gpa,
					u32 err, bool prefetch, int *emulation_type)
{
	struct kvm_page_fault fault = {
		.addr = cr2_or_gpa,
		.error_code = err,
		.exec = err & PFERR_FETCH_MASK,
		.write = err & PFERR_WRITE_MASK,
		.present = err & PFERR_PRESENT_MASK,
		.rsvd = err & PFERR_RSVD_MASK,
		.user = err & PFERR_USER_MASK,
		.prefetch = prefetch,
		.is_tdp = likely(vcpu->arch.mmu->page_fault == kvm_tdp_page_fault),
		.nx_huge_page_workaround_enabled =
			is_nx_huge_page_enabled(vcpu->kvm),
		.max_level = KVM_MAX_HUGEPAGE_LEVEL,
		.req_level = PG_LEVEL_4K,
		.goal_level = PG_LEVEL_4K,
	};
	int r;
	if (vcpu->arch.mmu->root_role.direct) {
		fault.gfn = fault.addr >> PAGE_SHIFT;
		fault.slot = kvm_vcpu_gfn_to_memslot(vcpu, fault.gfn);
	}
	if (!prefetch)
		vcpu->stat.pf_taken++;
	if (IS_ENABLED(CONFIG_RETPOLINE) && fault.is_tdp)
		r = kvm_tdp_page_fault(vcpu, &fault);
	else
		r = vcpu->arch.mmu->page_fault(vcpu, &fault);
	if (fault.write_fault_to_shadow_pgtable && emulation_type)
		*emulation_type |= EMULTYPE_WRITE_PF_TO_SP;
	if (r == RET_PF_FIXED)
		vcpu->stat.pf_fixed++;
	else if (prefetch)
		;
	else if (r == RET_PF_EMULATE)
		vcpu->stat.pf_emulate++;
	else if (r == RET_PF_SPURIOUS)
		vcpu->stat.pf_spurious++;
	return r;
}
int kvm_mmu_max_mapping_level(struct kvm *kvm,
			      const struct kvm_memory_slot *slot, gfn_t gfn,
			      int max_level);
void kvm_mmu_hugepage_adjust(struct kvm_vcpu *vcpu, struct kvm_page_fault *fault);
void disallowed_hugepage_adjust(struct kvm_page_fault *fault, u64 spte, int cur_level);
void *mmu_memory_cache_alloc(struct kvm_mmu_memory_cache *mc);
void track_possible_nx_huge_page(struct kvm *kvm, struct kvm_mmu_page *sp);
void untrack_possible_nx_huge_page(struct kvm *kvm, struct kvm_mmu_page *sp);
#endif  
