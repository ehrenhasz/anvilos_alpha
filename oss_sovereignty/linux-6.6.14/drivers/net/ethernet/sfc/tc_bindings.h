#ifndef EFX_TC_BINDINGS_H
#define EFX_TC_BINDINGS_H
#include "net_driver.h"
#if IS_ENABLED(CONFIG_SFC_SRIOV)
#include <net/sch_generic.h>
struct efx_rep;
void efx_tc_block_unbind(void *cb_priv);
int efx_tc_setup_block(struct net_device *net_dev, struct efx_nic *efx,
		       struct flow_block_offload *tcb, struct efx_rep *efv);
int efx_tc_setup(struct net_device *net_dev, enum tc_setup_type type,
		 void *type_data);
int efx_tc_indr_setup_cb(struct net_device *net_dev, struct Qdisc *sch,
			 void *cb_priv, enum tc_setup_type type,
			 void *type_data, void *data,
			 void (*cleanup)(struct flow_block_cb *block_cb));
int efx_tc_netdev_event(struct efx_nic *efx, unsigned long event,
			struct net_device *net_dev);
#else  
static inline int efx_tc_netdev_event(struct efx_nic *efx, unsigned long event,
				      struct net_device *net_dev)
{
	return NOTIFY_DONE;
}
#endif  
#endif  
