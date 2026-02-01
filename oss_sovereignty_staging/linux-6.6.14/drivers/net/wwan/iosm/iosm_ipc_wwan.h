 

#ifndef IOSM_IPC_WWAN_H
#define IOSM_IPC_WWAN_H

 
struct iosm_wwan *ipc_wwan_init(struct iosm_imem *ipc_imem, struct device *dev);

 
void ipc_wwan_deinit(struct iosm_wwan *ipc_wwan);

 
int ipc_wwan_receive(struct iosm_wwan *ipc_wwan, struct sk_buff *skb_arg,
		     bool dss, int if_id);

 
void ipc_wwan_tx_flowctrl(struct iosm_wwan *ipc_wwan, int id, bool on);
#endif
