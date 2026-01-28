#ifndef IOSM_IPC_PORT_H
#define IOSM_IPC_PORT_H
#include <linux/wwan.h>
#include "iosm_ipc_imem_ops.h"
struct iosm_cdev {
	struct wwan_port *iosm_port;
	struct iosm_imem *ipc_imem;
	struct device *dev;
	struct iosm_pcie *pcie;
	enum wwan_port_type port_type;
	struct ipc_mem_channel *channel;
	enum ipc_channel_id chl_id;
};
struct iosm_cdev *ipc_port_init(struct iosm_imem *ipc_imem,
				struct ipc_chnl_cfg ipc_port_cfg);
void ipc_port_deinit(struct iosm_cdev *ipc_port[]);
#endif
