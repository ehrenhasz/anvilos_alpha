#ifndef __KVM_VCPU_RISCV_TIMER_H
#define __KVM_VCPU_RISCV_TIMER_H
#include <linux/hrtimer.h>
struct kvm_guest_timer {
	u32 nsec_mult;
	u32 nsec_shift;
	u64 time_delta;
};
struct kvm_vcpu_timer {
	bool init_done;
	bool next_set;
	u64 next_cycles;
	struct hrtimer hrt;
	bool sstc_enabled;
	int (*timer_next_event)(struct kvm_vcpu *vcpu, u64 ncycles);
};
int kvm_riscv_vcpu_timer_next_event(struct kvm_vcpu *vcpu, u64 ncycles);
int kvm_riscv_vcpu_get_reg_timer(struct kvm_vcpu *vcpu,
				 const struct kvm_one_reg *reg);
int kvm_riscv_vcpu_set_reg_timer(struct kvm_vcpu *vcpu,
				 const struct kvm_one_reg *reg);
int kvm_riscv_vcpu_timer_init(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_timer_deinit(struct kvm_vcpu *vcpu);
int kvm_riscv_vcpu_timer_reset(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_timer_restore(struct kvm_vcpu *vcpu);
void kvm_riscv_guest_timer_init(struct kvm *kvm);
void kvm_riscv_vcpu_timer_sync(struct kvm_vcpu *vcpu);
void kvm_riscv_vcpu_timer_save(struct kvm_vcpu *vcpu);
bool kvm_riscv_vcpu_timer_pending(struct kvm_vcpu *vcpu);
#endif
