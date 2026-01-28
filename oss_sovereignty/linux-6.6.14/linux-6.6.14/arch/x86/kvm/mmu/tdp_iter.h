#ifndef __KVM_X86_MMU_TDP_ITER_H
#define __KVM_X86_MMU_TDP_ITER_H
#include <linux/kvm_host.h>
#include "mmu.h"
#include "spte.h"
static inline u64 kvm_tdp_mmu_read_spte(tdp_ptep_t sptep)
{
	return READ_ONCE(*rcu_dereference(sptep));
}
static inline u64 kvm_tdp_mmu_write_spte_atomic(tdp_ptep_t sptep, u64 new_spte)
{
	return xchg(rcu_dereference(sptep), new_spte);
}
static inline void __kvm_tdp_mmu_write_spte(tdp_ptep_t sptep, u64 new_spte)
{
	WRITE_ONCE(*rcu_dereference(sptep), new_spte);
}
static inline bool kvm_tdp_mmu_spte_need_atomic_write(u64 old_spte, int level)
{
	return is_shadow_present_pte(old_spte) &&
	       is_last_spte(old_spte, level) &&
	       spte_has_volatile_bits(old_spte);
}
static inline u64 kvm_tdp_mmu_write_spte(tdp_ptep_t sptep, u64 old_spte,
					 u64 new_spte, int level)
{
	if (kvm_tdp_mmu_spte_need_atomic_write(old_spte, level))
		return kvm_tdp_mmu_write_spte_atomic(sptep, new_spte);
	__kvm_tdp_mmu_write_spte(sptep, new_spte);
	return old_spte;
}
static inline u64 tdp_mmu_clear_spte_bits(tdp_ptep_t sptep, u64 old_spte,
					  u64 mask, int level)
{
	atomic64_t *sptep_atomic;
	if (kvm_tdp_mmu_spte_need_atomic_write(old_spte, level)) {
		sptep_atomic = (atomic64_t *)rcu_dereference(sptep);
		return (u64)atomic64_fetch_and(~mask, sptep_atomic);
	}
	__kvm_tdp_mmu_write_spte(sptep, old_spte & ~mask);
	return old_spte;
}
struct tdp_iter {
	gfn_t next_last_level_gfn;
	gfn_t yielded_gfn;
	tdp_ptep_t pt_path[PT64_ROOT_MAX_LEVEL];
	tdp_ptep_t sptep;
	gfn_t gfn;
	int root_level;
	int min_level;
	int level;
	int as_id;
	u64 old_spte;
	bool valid;
	bool yielded;
};
#define for_each_tdp_pte_min_level(iter, root, min_level, start, end) \
	for (tdp_iter_start(&iter, root, min_level, start); \
	     iter.valid && iter.gfn < end;		     \
	     tdp_iter_next(&iter))
#define for_each_tdp_pte(iter, root, start, end) \
	for_each_tdp_pte_min_level(iter, root, PG_LEVEL_4K, start, end)
tdp_ptep_t spte_to_child_pt(u64 pte, int level);
void tdp_iter_start(struct tdp_iter *iter, struct kvm_mmu_page *root,
		    int min_level, gfn_t next_last_level_gfn);
void tdp_iter_next(struct tdp_iter *iter);
void tdp_iter_restart(struct tdp_iter *iter);
#endif  
