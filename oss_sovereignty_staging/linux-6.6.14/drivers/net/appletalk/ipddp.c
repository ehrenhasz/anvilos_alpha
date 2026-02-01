 

#include <linux/compat.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/atalk.h>
#include <linux/if_arp.h>
#include <linux/slab.h>
#include <net/route.h>
#include <linux/uaccess.h>

#include "ipddp.h"		 

static const char version[] = KERN_INFO "ipddp.c:v0.01 8/28/97 Bradford W. Johnson <johns393@maroon.tc.umn.edu>\n";

static struct ipddp_route *ipddp_route_list;
static DEFINE_SPINLOCK(ipddp_route_lock);

#ifdef CONFIG_IPDDP_ENCAP
static int ipddp_mode = IPDDP_ENCAP;
#else
static int ipddp_mode = IPDDP_DECAP;
#endif

 
static netdev_tx_t ipddp_xmit(struct sk_buff *skb,
				    struct net_device *dev);
static int ipddp_create(struct ipddp_route *new_rt);
static int ipddp_delete(struct ipddp_route *rt);
static struct ipddp_route* __ipddp_find_route(struct ipddp_route *rt);
static int ipddp_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
				void __user *data, int cmd);

static const struct net_device_ops ipddp_netdev_ops = {
	.ndo_start_xmit		= ipddp_xmit,
	.ndo_siocdevprivate	= ipddp_siocdevprivate,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static struct net_device * __init ipddp_init(void)
{
	static unsigned version_printed;
	struct net_device *dev;
	int err;

	dev = alloc_etherdev(0);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	netif_keep_dst(dev);
	strcpy(dev->name, "ipddp%d");

	if (version_printed++ == 0)
                printk(version);

	 
	dev->netdev_ops = &ipddp_netdev_ops;

        dev->type = ARPHRD_IPDDP;       	 
        dev->mtu = 585;
        dev->flags |= IFF_NOARP;

         
        dev->hard_header_len = 14+8+sizeof(struct ddpehdr)+1;

	err = register_netdev(dev);
	if (err) {
		free_netdev(dev);
		return ERR_PTR(err);
	}

	 
	if(ipddp_mode == IPDDP_ENCAP)
		printk("%s: Appletalk-IP Encap. mode by Bradford W. Johnson <johns393@maroon.tc.umn.edu>\n", 
			dev->name);
	if(ipddp_mode == IPDDP_DECAP)
		printk("%s: Appletalk-IP Decap. mode by Jay Schulist <jschlst@samba.org>\n", 
			dev->name);

        return dev;
}


 
static netdev_tx_t ipddp_xmit(struct sk_buff *skb, struct net_device *dev)
{
        struct rtable *rtable = skb_rtable(skb);
        __be32 paddr = 0;
        struct ddpehdr *ddp;
        struct ipddp_route *rt;
        struct atalk_addr *our_addr;

	if (rtable->rt_gw_family == AF_INET)
		paddr = rtable->rt_gw4;

	spin_lock(&ipddp_route_lock);

	 
        for(rt = ipddp_route_list; rt != NULL; rt = rt->next)
        {
                if(rt->ip == paddr)
                        break;
        }
        if(rt == NULL) {
		spin_unlock(&ipddp_route_lock);
                return NETDEV_TX_OK;
	}

        our_addr = atalk_find_dev_addr(rt->dev);

	if(ipddp_mode == IPDDP_DECAP)
		 
		skb_pull(skb, 35-(sizeof(struct ddpehdr)+1));

	 
	ddp = (struct ddpehdr *)skb->data;
        ddp->deh_len_hops = htons(skb->len + (1<<10));
        ddp->deh_sum = 0;

	 
        if(rt->dev->type == ARPHRD_LOCALTLK)
        {
                ddp->deh_dnet  = 0;    
                ddp->deh_snet  = 0;
        }
        else
        {
                ddp->deh_dnet  = rt->at.s_net;    
                ddp->deh_snet  = our_addr->s_net;
        }
        ddp->deh_dnode = rt->at.s_node;
        ddp->deh_snode = our_addr->s_node;
        ddp->deh_dport = 72;
        ddp->deh_sport = 72;

        *((__u8 *)(ddp+1)) = 22;        	 

        skb->protocol = htons(ETH_P_ATALK);      

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	aarp_send_ddp(rt->dev, skb, &rt->at, NULL);

	spin_unlock(&ipddp_route_lock);

        return NETDEV_TX_OK;
}

 
static int ipddp_create(struct ipddp_route *new_rt)
{
        struct ipddp_route *rt = kzalloc(sizeof(*rt), GFP_KERNEL);

        if (rt == NULL)
                return -ENOMEM;

        rt->ip = new_rt->ip;
        rt->at = new_rt->at;
        rt->next = NULL;
        if ((rt->dev = atrtr_get_dev(&rt->at)) == NULL) {
		kfree(rt);
                return -ENETUNREACH;
        }

	spin_lock_bh(&ipddp_route_lock);
	if (__ipddp_find_route(rt)) {
		spin_unlock_bh(&ipddp_route_lock);
		kfree(rt);
		return -EEXIST;
	}

        rt->next = ipddp_route_list;
        ipddp_route_list = rt;

	spin_unlock_bh(&ipddp_route_lock);

        return 0;
}

 
static int ipddp_delete(struct ipddp_route *rt)
{
        struct ipddp_route **r = &ipddp_route_list;
        struct ipddp_route *tmp;

	spin_lock_bh(&ipddp_route_lock);
        while((tmp = *r) != NULL)
        {
                if(tmp->ip == rt->ip &&
		   tmp->at.s_net == rt->at.s_net &&
		   tmp->at.s_node == rt->at.s_node)
                {
                        *r = tmp->next;
			spin_unlock_bh(&ipddp_route_lock);
                        kfree(tmp);
                        return 0;
                }
                r = &tmp->next;
        }

	spin_unlock_bh(&ipddp_route_lock);
        return -ENOENT;
}

 
static struct ipddp_route* __ipddp_find_route(struct ipddp_route *rt)
{
        struct ipddp_route *f;

        for(f = ipddp_route_list; f != NULL; f = f->next)
        {
                if(f->ip == rt->ip &&
		   f->at.s_net == rt->at.s_net &&
		   f->at.s_node == rt->at.s_node)
                        return f;
        }

        return NULL;
}

static int ipddp_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
				void __user *data, int cmd)
{
        struct ipddp_route rcp, rcp2, *rp;

	if (in_compat_syscall())
		return -EOPNOTSUPP;

        if(!capable(CAP_NET_ADMIN))
                return -EPERM;

	if (copy_from_user(&rcp, data, sizeof(rcp)))
		return -EFAULT;

        switch(cmd)
        {
		case SIOCADDIPDDPRT:
                        return ipddp_create(&rcp);

                case SIOCFINDIPDDPRT:
			spin_lock_bh(&ipddp_route_lock);
			rp = __ipddp_find_route(&rcp);
			if (rp) {
				memset(&rcp2, 0, sizeof(rcp2));
				rcp2.ip    = rp->ip;
				rcp2.at    = rp->at;
				rcp2.flags = rp->flags;
			}
			spin_unlock_bh(&ipddp_route_lock);

			if (rp) {
				if (copy_to_user(data, &rcp2,
						 sizeof(struct ipddp_route)))
					return -EFAULT;
				return 0;
			} else
				return -ENOENT;

                case SIOCDELIPDDPRT:
                        return ipddp_delete(&rcp);

                default:
                        return -EINVAL;
        }
}

static struct net_device *dev_ipddp;

MODULE_LICENSE("GPL");
module_param(ipddp_mode, int, 0);

static int __init ipddp_init_module(void)
{
	dev_ipddp = ipddp_init();
	return PTR_ERR_OR_ZERO(dev_ipddp);
}

static void __exit ipddp_cleanup_module(void)
{
        struct ipddp_route *p;

	unregister_netdev(dev_ipddp);
        free_netdev(dev_ipddp);

        while (ipddp_route_list) {
                p = ipddp_route_list->next;
                kfree(ipddp_route_list);
                ipddp_route_list = p;
        }
}

module_init(ipddp_init_module);
module_exit(ipddp_cleanup_module);
