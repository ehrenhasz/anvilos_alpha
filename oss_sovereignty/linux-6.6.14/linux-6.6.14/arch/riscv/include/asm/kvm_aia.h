#ifndef __KVM_RISCV_AIA_H
#define __KVM_RISCV_AIA_H
#include <linux/jump_label.h>
#include <linux/kvm_types.h>
#include <asm/csr.h>
struct kvm_aia {
	bool		in_kernel;
	bool		initialized;
	u32		mode;
	u32		nr_ids;
	u32		nr_sources;
	u32		nr_group_bits;
	u32		nr_group_shift;
	u32		nr_hart_bits;
	u32		nr_guest_bits;
	gpa_t		aplic_addr;
	void		*aplic_state;
};
struct kvm_vcpu_aia_csr {
	unsigned long vsiselect;
	unsigned long hviprio1;
	unsigned long hviprio2;
	unsigned long vsieh;
	unsigned long hviph;
	unsigned long hviprio1h;
	unsigned long hviprio2h;
};
struct kvm_vcpu_aia {
	struct kvm_vcpu_aia_csr guest_csr;
	struct kvm_vcpu_aia_csr guest_reset_csr;
	gpa_t		imsic_addr;
	u32		hart_index;
	void		*imsic_state;
};
#define KVM_RISCV_AIA_UNDEF_ADDR	(-1)
#define kvm_riscv_aia_initialized(k)	((k)->arch.aia.initialized)
#define irqchip_in_kernel(k)		((k)->arch.aia.in_kernel)
extern unsigned int kvm_riscv_aia_nr_hgei;
extern unsigned int kvm_riscv_aia_max_ids;
DECLARE_STATIC_KEY_FALSE(kvm_riscv_aia_available);
#define kvm_riscv_aia_available() \
	static_branch_unlikely(&kvm_riscv_aia_available)
extern struct kvm_device_ops kvm_riscv_aia_device_ops;
void kvm_riscv_vcpu_aia_imsic_release(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_aia_imsic_update(struct kvm_vcpu *vcpu);
#define KVM_RISCV_AIA_IMSIC_TOPEI	(ISELECT_MASK + 1)
int kvm_riscv_vcpu_aia_imsic_rmw(struct kvm_vcpu *vcpu, unsigned long isel,
				 unsigned long *val, unsigned long new_val,
				 unsigned long wr_mask);
int kvm_riscv_aia_imsic_rw_attr(struct kvm *kvm, unsigned long type,
				bool write, unsigned long *val);
int kvm_riscv_aia_imsic_has_attr(struct kvm *kvm, unsigned long type);
void kvm_riscv_vcpu_aia_imsic_reset(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_aia_imsic_inject(struct kvm_vcpu *vcpu,
				    u32 guest_index, u32 offset, u32 iid);
int kvm_riscv_vcpu_aia_imsic_init(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_aia_imsic_cleanup(struct kvm_vcpu *vcpu);
int kvm_riscv_aia_aplic_set_attr(struct kvm *kvm, unsigned long type, u32 v);
int kvm_riscv_aia_aplic_get_attr(struct kvm *kvm, unsigned long type, u32 *v);
int kvm_riscv_aia_aplic_has_attr(struct kvm *kvm, unsigned long type);
int kvm_riscv_aia_aplic_inject(struct kvm *kvm, u32 source, bool level);
int kvm_riscv_aia_aplic_init(struct kvm *kvm);
void kvm_riscv_aia_aplic_cleanup(struct kvm *kvm);
#ifdef CONFIG_32BIT
void kvm_riscv_vcpu_aia_flush_interrupts(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_aia_sync_interrupts(struct kvm_vcpu *vcpu);
#else
static inline void kvm_riscv_vcpu_aia_flush_interrupts(struct kvm_vcpu *vcpu)
{
}
static inline void kvm_riscv_vcpu_aia_sync_interrupts(struct kvm_vcpu *vcpu)
{
}
#endif
bool kvm_riscv_vcpu_aia_has_interrupts(struct kvm_vcpu *vcpu, u64 mask);
void kvm_riscv_vcpu_aia_update_hvip(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_aia_load(struct kvm_vcpu *vcpu, int cpu);
void kvm_riscv_vcpu_aia_put(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_aia_get_csr(struct kvm_vcpu *vcpu,
			       unsigned long reg_num,
			       unsigned long *out_val);
int kvm_riscv_vcpu_aia_set_csr(struct kvm_vcpu *vcpu,
			       unsigned long reg_num,
			       unsigned long val);
int kvm_riscv_vcpu_aia_rmw_topei(struct kvm_vcpu *vcpu,
				 unsigned int csr_num,
				 unsigned long *val,
				 unsigned long new_val,
				 unsigned long wr_mask);
int kvm_riscv_vcpu_aia_rmw_ireg(struct kvm_vcpu *vcpu, unsigned int csr_num,
				unsigned long *val, unsigned long new_val,
				unsigned long wr_mask);
#define KVM_RISCV_VCPU_AIA_CSR_FUNCS \
{ .base = CSR_SIREG,      .count = 1, .func = kvm_riscv_vcpu_aia_rmw_ireg }, \
{ .base = CSR_STOPEI,     .count = 1, .func = kvm_riscv_vcpu_aia_rmw_topei },
int kvm_riscv_vcpu_aia_update(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_aia_reset(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_aia_init(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_aia_deinit(struct kvm_vcpu *vcpu);
int kvm_riscv_aia_inject_msi_by_id(struct kvm *kvm, u32 hart_index,
				   u32 guest_index, u32 iid);
int kvm_riscv_aia_inject_msi(struct kvm *kvm, struct kvm_msi *msi);
int kvm_riscv_aia_inject_irq(struct kvm *kvm, unsigned int irq, bool level);
void kvm_riscv_aia_init_vm(struct kvm *kvm);
void kvm_riscv_aia_destroy_vm(struct kvm *kvm);
int kvm_riscv_aia_alloc_hgei(int cpu, struct kvm_vcpu *owner,
			     void __iomem **hgei_va, phys_addr_t *hgei_pa);
void kvm_riscv_aia_free_hgei(int cpu, int hgei);
void kvm_riscv_aia_wakeon_hgei(struct kvm_vcpu *owner, bool enable);
void kvm_riscv_aia_enable(void);
void kvm_riscv_aia_disable(void);
int kvm_riscv_aia_init(void);
void kvm_riscv_aia_exit(void);
#endif
