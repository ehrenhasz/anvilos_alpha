 
 

 
#ifndef EF100_REP_H
#define EF100_REP_H

#include "net_driver.h"
#include "tc.h"

struct efx_rep_sw_stats {
	atomic64_t rx_packets, tx_packets;
	atomic64_t rx_bytes, tx_bytes;
	atomic64_t rx_dropped, tx_errors;
};

struct devlink_port;

 
struct efx_rep {
	struct efx_nic *parent;
	struct net_device *net_dev;
	u32 msg_enable;
	u32 mport;
	unsigned int idx;
	unsigned int write_index, read_index;
	unsigned int rx_pring_size;
	struct efx_tc_flow_rule dflt;
	struct list_head list;
	struct list_head rx_list;
	spinlock_t rx_lock;
	struct napi_struct napi;
	struct efx_rep_sw_stats stats;
	struct devlink_port *dl_port;
};

int efx_ef100_vfrep_create(struct efx_nic *efx, unsigned int i);
void efx_ef100_vfrep_destroy(struct efx_nic *efx, struct efx_rep *efv);
void efx_ef100_fini_vfreps(struct efx_nic *efx);

void efx_ef100_rep_rx_packet(struct efx_rep *efv, struct efx_rx_buffer *rx_buf);
 
struct efx_rep *efx_ef100_find_rep_by_mport(struct efx_nic *efx, u16 mport);
extern const struct net_device_ops efx_ef100_rep_netdev_ops;
void efx_ef100_init_reps(struct efx_nic *efx);
void efx_ef100_fini_reps(struct efx_nic *efx);
struct mae_mport_desc;
bool ef100_mport_on_local_intf(struct efx_nic *efx,
			       struct mae_mport_desc *mport_desc);
bool ef100_mport_is_vf(struct mae_mport_desc *mport_desc);
#endif  
