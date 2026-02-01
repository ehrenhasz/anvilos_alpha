 
 

#ifndef __IRQ_H
#define __IRQ_H

#include <linux/mm_types.h>
#include <linux/hrtimer.h>
#include <linux/kvm_host.h>
#include <linux/spinlock.h>

#include <kvm/iodev.h>
#include "lapic.h"

#define PIC_NUM_PINS 16
#define SELECT_PIC(irq) \
	((irq) < 8 ? KVM_IRQCHIP_PIC_MASTER : KVM_IRQCHIP_PIC_SLAVE)

struct kvm;
struct kvm_vcpu;

struct kvm_kpic_state {
	u8 last_irr;	 
	u8 irr;		 
	u8 imr;		 
	u8 isr;		 
	u8 priority_add;	 
	u8 irq_base;
	u8 read_reg_select;
	u8 poll;
	u8 special_mask;
	u8 init_state;
	u8 auto_eoi;
	u8 rotate_on_auto_eoi;
	u8 special_fully_nested_mode;
	u8 init4;		 
	u8 elcr;		 
	u8 elcr_mask;
	u8 isr_ack;	 
	struct kvm_pic *pics_state;
};

struct kvm_pic {
	spinlock_t lock;
	bool wakeup_needed;
	unsigned pending_acks;
	struct kvm *kvm;
	struct kvm_kpic_state pics[2];  
	int output;		 
	struct kvm_io_device dev_master;
	struct kvm_io_device dev_slave;
	struct kvm_io_device dev_elcr;
	unsigned long irq_states[PIC_NUM_PINS];
};

int kvm_pic_init(struct kvm *kvm);
void kvm_pic_destroy(struct kvm *kvm);
int kvm_pic_read_irq(struct kvm *kvm);
void kvm_pic_update_irq(struct kvm_pic *s);

static inline int irqchip_split(struct kvm *kvm)
{
	int mode = kvm->arch.irqchip_mode;

	 
	smp_rmb();
	return mode == KVM_IRQCHIP_SPLIT;
}

static inline int irqchip_kernel(struct kvm *kvm)
{
	int mode = kvm->arch.irqchip_mode;

	 
	smp_rmb();
	return mode == KVM_IRQCHIP_KERNEL;
}

static inline int pic_in_kernel(struct kvm *kvm)
{
	return irqchip_kernel(kvm);
}

static inline int irqchip_in_kernel(struct kvm *kvm)
{
	int mode = kvm->arch.irqchip_mode;

	 
	smp_rmb();
	return mode != KVM_IRQCHIP_NONE;
}

void kvm_inject_pending_timer_irqs(struct kvm_vcpu *vcpu);
void kvm_inject_apic_timer_irqs(struct kvm_vcpu *vcpu);
void kvm_apic_nmi_wd_deliver(struct kvm_vcpu *vcpu);
void __kvm_migrate_apic_timer(struct kvm_vcpu *vcpu);
void __kvm_migrate_pit_timer(struct kvm_vcpu *vcpu);
void __kvm_migrate_timers(struct kvm_vcpu *vcpu);

int apic_has_pending_timer(struct kvm_vcpu *vcpu);

int kvm_setup_default_irq_routing(struct kvm *kvm);
int kvm_setup_empty_irq_routing(struct kvm *kvm);
int kvm_irq_delivery_to_apic(struct kvm *kvm, struct kvm_lapic *src,
			     struct kvm_lapic_irq *irq,
			     struct dest_map *dest_map);

#endif
