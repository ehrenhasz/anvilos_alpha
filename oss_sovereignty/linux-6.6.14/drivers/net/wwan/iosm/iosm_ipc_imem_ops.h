 

#ifndef IOSM_IPC_IMEM_OPS_H
#define IOSM_IPC_IMEM_OPS_H

#include "iosm_ipc_mux_codec.h"

 
#define IPC_READ_TIMEOUT 3000

 
#define SIO_UNREGISTER_DEFER_DELAY_MS 1

 
#define PSI_START_DEFAULT_TIMEOUT 3000

 
#define BOOT_CHECK_DEFAULT_TIMEOUT 400

 
#define IP_MUX_SESSION_START 0
#define IP_MUX_SESSION_END 7

 
#define IP_MUX_SESSION_DEFAULT	0

 
struct ipc_mem_channel *ipc_imem_sys_port_open(struct iosm_imem *ipc_imem,
					       int chl_id, int hp_id);
void ipc_imem_sys_port_close(struct iosm_imem *ipc_imem,
			     struct ipc_mem_channel *channel);

 
int ipc_imem_sys_cdev_write(struct iosm_cdev *ipc_cdev, struct sk_buff *skb);

 
int ipc_imem_sys_wwan_open(struct iosm_imem *ipc_imem, int if_id);

 
void ipc_imem_sys_wwan_close(struct iosm_imem *ipc_imem, int if_id,
			     int channel_id);

 
int ipc_imem_sys_wwan_transmit(struct iosm_imem *ipc_imem, int if_id,
			       int channel_id, struct sk_buff *skb);
 
int ipc_imem_wwan_channel_init(struct iosm_imem *ipc_imem,
			       enum ipc_mux_protocol mux_type);

 
struct ipc_mem_channel *ipc_imem_sys_devlink_open(struct iosm_imem *ipc_imem);

 
void ipc_imem_sys_devlink_close(struct iosm_devlink *ipc_devlink);

 
void ipc_imem_sys_devlink_notify_rx(struct iosm_devlink *ipc_devlink,
				    struct sk_buff *skb);

 
int ipc_imem_sys_devlink_read(struct iosm_devlink *ipc_devlink, u8 *data,
			      u32 bytes_to_read, u32 *bytes_read);

 
int ipc_imem_sys_devlink_write(struct iosm_devlink *ipc_devlink,
			       unsigned char *buf, int count);

#endif
