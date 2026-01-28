


#include <linux/netdevice.h>
#include "ef100_rep.h"

netdev_tx_t __ef100_hard_start_xmit(struct sk_buff *skb,
				    struct efx_nic *efx,
				    struct net_device *net_dev,
				    struct efx_rep *efv);
int ef100_netdev_event(struct notifier_block *this,
		       unsigned long event, void *ptr);
int ef100_probe_netdev(struct efx_probe_data *probe_data);
void ef100_remove_netdev(struct efx_probe_data *probe_data);
