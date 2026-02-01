 

#ifndef __OPA_SMI_H_
#define __OPA_SMI_H_

#include <rdma/ib_smi.h>
#include <rdma/opa_smi.h>

#include "smi.h"

enum smi_action opa_smi_handle_dr_smp_recv(struct opa_smp *smp, bool is_switch,
				       u32 port_num, int phys_port_cnt);
int opa_smi_get_fwd_port(struct opa_smp *smp);
extern enum smi_forward_action opa_smi_check_forward_dr_smp(struct opa_smp *smp);
extern enum smi_action opa_smi_handle_dr_smp_send(struct opa_smp *smp,
					      bool is_switch, u32 port_num);

 
static inline enum smi_action opa_smi_check_local_smp(struct opa_smp *smp,
						      struct ib_device *device)
{
	 
	 
	return (device->ops.process_mad &&
		!opa_get_smp_direction(smp) &&
		(smp->hop_ptr == smp->hop_cnt + 1)) ?
		IB_SMI_HANDLE : IB_SMI_DISCARD;
}

 
static inline enum smi_action opa_smi_check_local_returning_smp(struct opa_smp *smp,
								struct ib_device *device)
{
	 
	 
	return (device->ops.process_mad &&
		opa_get_smp_direction(smp) &&
		!smp->hop_ptr) ? IB_SMI_HANDLE : IB_SMI_DISCARD;
}

#endif	 
