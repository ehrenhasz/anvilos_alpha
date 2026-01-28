#ifndef __ARM64_KVM_MMU_H__
#define __ARM64_KVM_MMU_H__
#include <asm/page.h>
#include <asm/memory.h>
#include <asm/mmu.h>
#include <asm/cpufeature.h>
#ifdef __ASSEMBLY__
#include <asm/alternative.h>
.macro kern_hyp_va	reg
#ifndef __KVM_VHE_HYPERVISOR__
alternative_cb ARM64_ALWAYS_SYSTEM, kvm_update_va_mask
	and     \reg, \reg, #1		 
	ror	\reg, \reg, #1		 
	add	\reg, \reg, #0		 
	add	\reg, \reg, #0, lsl 12	 
	ror	\reg, \reg, #63		 
alternative_cb_end
#endif
.endm
.macro hyp_pa reg, tmp
	ldr_l	\tmp, hyp_physvirt_offset
	add	\reg, \reg, \tmp
.endm
.macro hyp_kimg_va reg, tmp
	hyp_pa	\reg, \tmp
alternative_cb ARM64_ALWAYS_SYSTEM, kvm_get_kimage_voffset
	movz	\tmp, #0
	movk	\tmp, #0, lsl #16
	movk	\tmp, #0, lsl #32
	movk	\tmp, #0, lsl #48
alternative_cb_end
	add	\reg, \reg, \tmp
.endm
#else
#include <linux/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_host.h>
void kvm_update_va_mask(struct alt_instr *alt,
			__le32 *origptr, __le32 *updptr, int nr_inst);
void kvm_compute_layout(void);
void kvm_apply_hyp_relocations(void);
#define __hyp_pa(x) (((phys_addr_t)(x)) + hyp_physvirt_offset)
static __always_inline unsigned long __kern_hyp_va(unsigned long v)
{
#ifndef __KVM_VHE_HYPERVISOR__
	asm volatile(ALTERNATIVE_CB("and %0, %0, #1\n"
				    "ror %0, %0, #1\n"
				    "add %0, %0, #0\n"
				    "add %0, %0, #0, lsl 12\n"
				    "ror %0, %0, #63\n",
				    ARM64_ALWAYS_SYSTEM,
				    kvm_update_va_mask)
		     : "+r" (v));
#endif
	return v;
}
#define kern_hyp_va(v) 	((typeof(v))(__kern_hyp_va((unsigned long)(v))))
#define KVM_PHYS_SHIFT	(40)
#define kvm_phys_shift(kvm)		VTCR_EL2_IPA(kvm->arch.vtcr)
#define kvm_phys_size(kvm)		(_AC(1, ULL) << kvm_phys_shift(kvm))
#define kvm_phys_mask(kvm)		(kvm_phys_size(kvm) - _AC(1, ULL))
#include <asm/kvm_pgtable.h>
#include <asm/stage2_pgtable.h>
int kvm_share_hyp(void *from, void *to);
void kvm_unshare_hyp(void *from, void *to);
int create_hyp_mappings(void *from, void *to, enum kvm_pgtable_prot prot);
int __create_hyp_mappings(unsigned long start, unsigned long size,
			  unsigned long phys, enum kvm_pgtable_prot prot);
int hyp_alloc_private_va_range(size_t size, unsigned long *haddr);
int create_hyp_io_mappings(phys_addr_t phys_addr, size_t size,
			   void __iomem **kaddr,
			   void __iomem **haddr);
int create_hyp_exec_mappings(phys_addr_t phys_addr, size_t size,
			     void **haddr);
int create_hyp_stack(phys_addr_t phys_addr, unsigned long *haddr);
void __init free_hyp_pgds(void);
void stage2_unmap_vm(struct kvm *kvm);
int kvm_init_stage2_mmu(struct kvm *kvm, struct kvm_s2_mmu *mmu, unsigned long type);
void kvm_uninit_stage2_mmu(struct kvm *kvm);
void kvm_free_stage2_pgd(struct kvm_s2_mmu *mmu);
int kvm_phys_addr_ioremap(struct kvm *kvm, phys_addr_t guest_ipa,
			  phys_addr_t pa, unsigned long size, bool writable);
int kvm_handle_guest_abort(struct kvm_vcpu *vcpu);
phys_addr_t kvm_mmu_get_httbr(void);
phys_addr_t kvm_get_idmap_vector(void);
int __init kvm_mmu_init(u32 *hyp_va_bits);
static inline void *__kvm_vector_slot2addr(void *base,
					   enum arm64_hyp_spectre_vector slot)
{
	int idx = slot - (slot != HYP_VECTOR_DIRECT);
	return base + (idx * SZ_2K);
}
struct kvm;
#define kvm_flush_dcache_to_poc(a,l)	\
	dcache_clean_inval_poc((unsigned long)(a), (unsigned long)(a)+(l))
static inline bool vcpu_has_cache_enabled(struct kvm_vcpu *vcpu)
{
	u64 cache_bits = SCTLR_ELx_M | SCTLR_ELx_C;
	int reg;
	if (vcpu_is_el2(vcpu))
		reg = SCTLR_EL2;
	else
		reg = SCTLR_EL1;
	return (vcpu_read_sys_reg(vcpu, reg) & cache_bits) == cache_bits;
}
static inline void __clean_dcache_guest_page(void *va, size_t size)
{
	if (cpus_have_const_cap(ARM64_HAS_STAGE2_FWB))
		return;
	kvm_flush_dcache_to_poc(va, size);
}
static inline void __invalidate_icache_guest_page(void *va, size_t size)
{
	if (icache_is_aliasing()) {
		icache_inval_all_pou();
	} else if (read_sysreg(CurrentEL) != CurrentEL_EL1 ||
		   !icache_is_vpipt()) {
		icache_inval_pou((unsigned long)va, (unsigned long)va + size);
	}
}
void kvm_set_way_flush(struct kvm_vcpu *vcpu);
void kvm_toggle_cache(struct kvm_vcpu *vcpu, bool was_enabled);
static inline unsigned int kvm_get_vmid_bits(void)
{
	int reg = read_sanitised_ftr_reg(SYS_ID_AA64MMFR1_EL1);
	return get_vmid_bits(reg);
}
static inline int kvm_read_guest_lock(struct kvm *kvm,
				      gpa_t gpa, void *data, unsigned long len)
{
	int srcu_idx = srcu_read_lock(&kvm->srcu);
	int ret = kvm_read_guest(kvm, gpa, data, len);
	srcu_read_unlock(&kvm->srcu, srcu_idx);
	return ret;
}
static inline int kvm_write_guest_lock(struct kvm *kvm, gpa_t gpa,
				       const void *data, unsigned long len)
{
	int srcu_idx = srcu_read_lock(&kvm->srcu);
	int ret = kvm_write_guest(kvm, gpa, data, len);
	srcu_read_unlock(&kvm->srcu, srcu_idx);
	return ret;
}
#define kvm_phys_to_vttbr(addr)		phys_to_ttbr(addr)
static __always_inline u64 kvm_get_vttbr(struct kvm_s2_mmu *mmu)
{
	struct kvm_vmid *vmid = &mmu->vmid;
	u64 vmid_field, baddr;
	u64 cnp = system_supports_cnp() ? VTTBR_CNP_BIT : 0;
	baddr = mmu->pgd_phys;
	vmid_field = atomic64_read(&vmid->id) << VTTBR_VMID_SHIFT;
	vmid_field &= VTTBR_VMID_MASK(kvm_arm_vmid_bits);
	return kvm_phys_to_vttbr(baddr) | vmid_field | cnp;
}
static __always_inline void __load_stage2(struct kvm_s2_mmu *mmu,
					  struct kvm_arch *arch)
{
	write_sysreg(arch->vtcr, vtcr_el2);
	write_sysreg(kvm_get_vttbr(mmu), vttbr_el2);
	asm(ALTERNATIVE("nop", "isb", ARM64_WORKAROUND_SPECULATIVE_AT));
}
static inline struct kvm *kvm_s2_mmu_to_kvm(struct kvm_s2_mmu *mmu)
{
	return container_of(mmu->arch, struct kvm, arch);
}
#endif  
#endif  
