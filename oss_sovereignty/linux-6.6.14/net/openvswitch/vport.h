 
 

#ifndef VPORT_H
#define VPORT_H 1

#include <linux/if_tunnel.h>
#include <linux/list.h>
#include <linux/netlink.h>
#include <linux/openvswitch.h>
#include <linux/reciprocal_div.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/u64_stats_sync.h>

#include "datapath.h"

struct vport;
struct vport_parms;

 

int ovs_vport_init(void);
void ovs_vport_exit(void);

struct vport *ovs_vport_add(const struct vport_parms *);
void ovs_vport_del(struct vport *);

struct vport *ovs_vport_locate(const struct net *net, const char *name);

void ovs_vport_get_stats(struct vport *, struct ovs_vport_stats *);

int ovs_vport_get_upcall_stats(struct vport *vport, struct sk_buff *skb);

int ovs_vport_set_options(struct vport *, struct nlattr *options);
int ovs_vport_get_options(const struct vport *, struct sk_buff *);

int ovs_vport_set_upcall_portids(struct vport *, const struct nlattr *pids);
int ovs_vport_get_upcall_portids(const struct vport *, struct sk_buff *);
u32 ovs_vport_find_upcall_portid(const struct vport *, struct sk_buff *);

 
struct vport_portids {
	struct reciprocal_value rn_ids;
	struct rcu_head rcu;
	u32 n_ids;
	u32 ids[];
};

 
struct vport {
	struct net_device *dev;
	netdevice_tracker dev_tracker;
	struct datapath	*dp;
	struct vport_portids __rcu *upcall_portids;
	u16 port_no;

	struct hlist_node hash_node;
	struct hlist_node dp_hash_node;
	const struct vport_ops *ops;
	struct vport_upcall_stats_percpu __percpu *upcall_stats;

	struct list_head detach_list;
	struct rcu_head rcu;
};

 
struct vport_parms {
	const char *name;
	enum ovs_vport_type type;
	int desired_ifindex;
	struct nlattr *options;

	 
	struct datapath *dp;
	u16 port_no;
	struct nlattr *upcall_portids;
};

 
struct vport_ops {
	enum ovs_vport_type type;

	 
	struct vport *(*create)(const struct vport_parms *);
	void (*destroy)(struct vport *);

	int (*set_options)(struct vport *, struct nlattr *);
	int (*get_options)(const struct vport *, struct sk_buff *);

	int (*send)(struct sk_buff *skb);
	struct module *owner;
	struct list_head list;
};

 
struct vport_upcall_stats_percpu {
	struct u64_stats_sync syncp;
	u64_stats_t n_success;
	u64_stats_t n_fail;
};

struct vport *ovs_vport_alloc(int priv_size, const struct vport_ops *,
			      const struct vport_parms *);
void ovs_vport_free(struct vport *);

#define VPORT_ALIGN 8

 
static inline void *vport_priv(const struct vport *vport)
{
	return (u8 *)(uintptr_t)vport + ALIGN(sizeof(struct vport), VPORT_ALIGN);
}

 
static inline struct vport *vport_from_priv(void *priv)
{
	return (struct vport *)((u8 *)priv - ALIGN(sizeof(struct vport), VPORT_ALIGN));
}

int ovs_vport_receive(struct vport *, struct sk_buff *,
		      const struct ip_tunnel_info *);

static inline const char *ovs_vport_name(struct vport *vport)
{
	return vport->dev->name;
}

int __ovs_vport_ops_register(struct vport_ops *ops);
#define ovs_vport_ops_register(ops)		\
	({					\
		(ops)->owner = THIS_MODULE;	\
		__ovs_vport_ops_register(ops);	\
	})

void ovs_vport_ops_unregister(struct vport_ops *ops);
void ovs_vport_send(struct vport *vport, struct sk_buff *skb, u8 mac_proto);

#endif  
