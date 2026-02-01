 

#ifndef __SMI_H_
#define __SMI_H_

#include <rdma/ib_smi.h>

enum smi_action {
	IB_SMI_DISCARD,
	IB_SMI_HANDLE
};

enum smi_forward_action {
	IB_SMI_LOCAL,	 
	IB_SMI_SEND,	 
	IB_SMI_FORWARD	 
};

enum smi_action smi_handle_dr_smp_recv(struct ib_smp *smp, bool is_switch,
				       u32 port_num, int phys_port_cnt);
int smi_get_fwd_port(struct ib_smp *smp);
extern enum smi_forward_action smi_check_forward_dr_smp(struct ib_smp *smp);
extern enum smi_action smi_handle_dr_smp_send(struct ib_smp *smp,
					      bool is_switch, u32 port_num);

 
static inline enum smi_action smi_check_local_smp(struct ib_smp *smp,
						  struct ib_device *device)
{
	 
	 
	return ((device->ops.process_mad &&
		!ib_get_smp_direction(smp) &&
		(smp->hop_ptr == smp->hop_cnt + 1)) ?
		IB_SMI_HANDLE : IB_SMI_DISCARD);
}

 
static inline enum smi_action smi_check_local_returning_smp(struct ib_smp *smp,
						   struct ib_device *device)
{
	 
	 
	return ((device->ops.process_mad &&
		ib_get_smp_direction(smp) &&
		!smp->hop_ptr) ? IB_SMI_HANDLE : IB_SMI_DISCARD);
}

#endif	 
