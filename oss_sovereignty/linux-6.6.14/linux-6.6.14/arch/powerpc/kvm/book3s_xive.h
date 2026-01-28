#ifndef _KVM_PPC_BOOK3S_XIVE_H
#define _KVM_PPC_BOOK3S_XIVE_H
#ifdef CONFIG_KVM_XICS
#include "book3s_xics.h"
#define KVMPPC_XIVE_FIRST_IRQ	0
#define KVMPPC_XIVE_NR_IRQS	KVMPPC_XICS_NR_IRQS
struct kvmppc_xive_irq_state {
	bool valid;			 
	u32 number;			 
	u32 ipi_number;			 
	struct xive_irq_data ipi_data;	 
	u32 pt_number;			 
	struct xive_irq_data *pt_data;	 
	u8 guest_priority;		 
	u8 saved_priority;		 
	u32 act_server;			 
	u8 act_priority;		 
	bool in_eoi;			 
	bool old_p;			 
	bool old_q;			 
	bool lsi;			 
	bool asserted;			 
	bool in_queue;
	bool saved_p;
	bool saved_q;
	u8 saved_scan_prio;
	u32 eisn;			 
};
static inline void kvmppc_xive_select_irq(struct kvmppc_xive_irq_state *state,
					  u32 *out_hw_irq,
					  struct xive_irq_data **out_xd)
{
	if (state->pt_number) {
		if (out_hw_irq)
			*out_hw_irq = state->pt_number;
		if (out_xd)
			*out_xd = state->pt_data;
	} else {
		if (out_hw_irq)
			*out_hw_irq = state->ipi_number;
		if (out_xd)
			*out_xd = &state->ipi_data;
	}
}
struct kvmppc_xive_src_block {
	arch_spinlock_t lock;
	u16 id;
	struct kvmppc_xive_irq_state irq_state[KVMPPC_XICS_IRQ_PER_ICS];
};
struct kvmppc_xive;
struct kvmppc_xive_ops {
	int (*reset_mapped)(struct kvm *kvm, unsigned long guest_irq);
};
#define KVMPPC_XIVE_FLAG_SINGLE_ESCALATION 0x1
#define KVMPPC_XIVE_FLAG_SAVE_RESTORE 0x2
struct kvmppc_xive {
	struct kvm *kvm;
	struct kvm_device *dev;
	struct dentry *dentry;
	u32	vp_base;
	struct kvmppc_xive_src_block *src_blocks[KVMPPC_XICS_MAX_ICS_ID + 1];
	u32	max_sbid;
	u32	src_count;
	u32	saved_src_count;
	u32	delayed_irqs;
	u8	qmap;
	u32	q_order;
	u32	q_page_order;
	u8	flags;
	u32	nr_servers;
	struct kvmppc_xive_ops *ops;
	struct address_space   *mapping;
	struct mutex mapping_lock;
	struct mutex lock;
};
#define KVMPPC_XIVE_Q_COUNT	8
struct kvmppc_xive_vcpu {
	struct kvmppc_xive	*xive;
	struct kvm_vcpu		*vcpu;
	bool			valid;
	u32			server_num;
	u32			vp_id;
	u32			vp_chip_id;
	u32			vp_cam;
	u32			vp_ipi;
	struct xive_irq_data	vp_ipi_data;
	uint8_t			cppr;	 
	uint8_t			hw_cppr; 
	uint8_t			mfrr;
	uint8_t			pending;
	struct xive_q		queues[KVMPPC_XIVE_Q_COUNT];
	u32			esc_virq[KVMPPC_XIVE_Q_COUNT];
	char			*esc_virq_names[KVMPPC_XIVE_Q_COUNT];
	u32			delayed_irq;
	u64			stat_rm_h_xirr;
	u64			stat_rm_h_ipoll;
	u64			stat_rm_h_cppr;
	u64			stat_rm_h_eoi;
	u64			stat_rm_h_ipi;
	u64			stat_vm_h_xirr;
	u64			stat_vm_h_ipoll;
	u64			stat_vm_h_cppr;
	u64			stat_vm_h_eoi;
	u64			stat_vm_h_ipi;
};
static inline struct kvm_vcpu *kvmppc_xive_find_server(struct kvm *kvm, u32 nr)
{
	struct kvm_vcpu *vcpu = NULL;
	unsigned long i;
	kvm_for_each_vcpu(i, vcpu, kvm) {
		if (vcpu->arch.xive_vcpu && nr == vcpu->arch.xive_vcpu->server_num)
			return vcpu;
	}
	return NULL;
}
static inline struct kvmppc_xive_src_block *kvmppc_xive_find_source(struct kvmppc_xive *xive,
		u32 irq, u16 *source)
{
	u32 bid = irq >> KVMPPC_XICS_ICS_SHIFT;
	u16 src = irq & KVMPPC_XICS_SRC_MASK;
	if (source)
		*source = src;
	if (bid > KVMPPC_XICS_MAX_ICS_ID)
		return NULL;
	return xive->src_blocks[bid];
}
static inline u32 kvmppc_xive_vp(struct kvmppc_xive *xive, u32 server)
{
	return xive->vp_base + kvmppc_pack_vcpu_id(xive->kvm, server);
}
static inline bool kvmppc_xive_vp_in_use(struct kvm *kvm, u32 vp_id)
{
	struct kvm_vcpu *vcpu = NULL;
	unsigned long i;
	kvm_for_each_vcpu(i, vcpu, kvm) {
		if (vcpu->arch.xive_vcpu && vp_id == vcpu->arch.xive_vcpu->vp_id)
			return true;
	}
	return false;
}
static inline u8 xive_prio_from_guest(u8 prio)
{
	if (prio == 0xff || prio < 6)
		return prio;
	return 6;
}
static inline u8 xive_prio_to_guest(u8 prio)
{
	return prio;
}
static inline u32 __xive_read_eq(__be32 *qpage, u32 msk, u32 *idx, u32 *toggle)
{
	u32 cur;
	if (!qpage)
		return 0;
	cur = be32_to_cpup(qpage + *idx);
	if ((cur >> 31) == *toggle)
		return 0;
	*idx = (*idx + 1) & msk;
	if (*idx == 0)
		(*toggle) ^= 1;
	return cur & 0x7fffffff;
}
void kvmppc_xive_disable_vcpu_interrupts(struct kvm_vcpu *vcpu);
int kvmppc_xive_debug_show_queues(struct seq_file *m, struct kvm_vcpu *vcpu);
void kvmppc_xive_debug_show_sources(struct seq_file *m,
				    struct kvmppc_xive_src_block *sb);
struct kvmppc_xive_src_block *kvmppc_xive_create_src_block(
	struct kvmppc_xive *xive, int irq);
void kvmppc_xive_free_sources(struct kvmppc_xive_src_block *sb);
int kvmppc_xive_select_target(struct kvm *kvm, u32 *server, u8 prio);
int kvmppc_xive_attach_escalation(struct kvm_vcpu *vcpu, u8 prio,
				  bool single_escalation);
struct kvmppc_xive *kvmppc_xive_get_device(struct kvm *kvm, u32 type);
void xive_cleanup_single_escalation(struct kvm_vcpu *vcpu, int irq);
int kvmppc_xive_compute_vp_id(struct kvmppc_xive *xive, u32 cpu, u32 *vp);
int kvmppc_xive_set_nr_servers(struct kvmppc_xive *xive, u64 addr);
bool kvmppc_xive_check_save_restore(struct kvm_vcpu *vcpu);
static inline bool kvmppc_xive_has_single_escalation(struct kvmppc_xive *xive)
{
	return xive->flags & KVMPPC_XIVE_FLAG_SINGLE_ESCALATION;
}
#endif  
#endif  
