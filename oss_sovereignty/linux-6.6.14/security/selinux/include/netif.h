#ifndef _SELINUX_NETIF_H_
#define _SELINUX_NETIF_H_
#include <net/net_namespace.h>
void sel_netif_flush(void);
int sel_netif_sid(struct net *ns, int ifindex, u32 *sid);
#endif	 
