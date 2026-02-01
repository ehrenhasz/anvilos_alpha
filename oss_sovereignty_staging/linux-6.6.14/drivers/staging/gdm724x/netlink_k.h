 
 

#ifndef _NETLINK_K_H
#define _NETLINK_K_H

#include <linux/netdevice.h>
#include <net/sock.h>

struct sock *netlink_init(int unit,
			  void (*cb)(struct net_device *dev,
				     u16 type, void *msg, int len));
int netlink_send(struct sock *sock, int group, u16 type, void *msg, int len,
		 struct net_device *dev);

#endif  
