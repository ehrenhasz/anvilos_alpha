#ifndef __ARM64_S2_PGTABLE_H_
#define __ARM64_S2_PGTABLE_H_
#include <linux/pgtable.h>
#define stage2_pgtable_levels(ipa)	ARM64_HW_PGTABLE_LEVELS((ipa) - 4)
#define kvm_stage2_levels(kvm)		VTCR_EL2_LVLS(kvm->arch.vtcr)
#define kvm_mmu_cache_min_pages(kvm)	(kvm_stage2_levels(kvm) - 1)
#endif	 
