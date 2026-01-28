#ifndef DATAPATH_H
#define DATAPATH_H 1
#include <asm/page.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/u64_stats_sync.h>
#include <net/ip_tunnels.h>
#include "conntrack.h"
#include "flow.h"
#include "flow_table.h"
#include "meter.h"
#include "vport-internal_dev.h"
#define DP_MAX_PORTS                USHRT_MAX
#define DP_VPORT_HASH_BUCKETS       1024
#define DP_MASKS_REBALANCE_INTERVAL 4000
struct dp_stats_percpu {
	u64 n_hit;
	u64 n_missed;
	u64 n_lost;
	u64 n_mask_hit;
	u64 n_cache_hit;
	struct u64_stats_sync syncp;
};
struct dp_nlsk_pids {
	struct rcu_head rcu;
	u32 n_pids;
	u32 pids[];
};
struct datapath {
	struct rcu_head rcu;
	struct list_head list_node;
	struct flow_table table;
	struct hlist_head *ports;
	struct dp_stats_percpu __percpu *stats_percpu;
	possible_net_t net;
	u32 user_features;
	u32 max_headroom;
	struct dp_meter_table meter_tbl;
	struct dp_nlsk_pids __rcu *upcall_portids;
};
struct ovs_skb_cb {
	struct vport		*input_vport;
	u16			mru;
	u16			acts_origlen;
	u32			cutlen;
};
#define OVS_CB(skb) ((struct ovs_skb_cb *)(skb)->cb)
struct dp_upcall_info {
	struct ip_tunnel_info *egress_tun_info;
	const struct nlattr *userdata;
	const struct nlattr *actions;
	int actions_len;
	u32 portid;
	u8 cmd;
	u16 mru;
};
struct ovs_net {
	struct list_head dps;
	struct work_struct dp_notify_work;
	struct delayed_work masks_rebalance;
#if	IS_ENABLED(CONFIG_NETFILTER_CONNCOUNT)
	struct ovs_ct_limit_info *ct_limit_info;
#endif
	bool xt_label;
};
enum ovs_pkt_hash_types {
	OVS_PACKET_HASH_SW_BIT = (1ULL << 32),
	OVS_PACKET_HASH_L4_BIT = (1ULL << 33),
};
extern unsigned int ovs_net_id;
void ovs_lock(void);
void ovs_unlock(void);
#ifdef CONFIG_LOCKDEP
int lockdep_ovsl_is_held(void);
#else
#define lockdep_ovsl_is_held()	1
#endif
#define ASSERT_OVSL()		WARN_ON(!lockdep_ovsl_is_held())
#define ovsl_dereference(p)					\
	rcu_dereference_protected(p, lockdep_ovsl_is_held())
#define rcu_dereference_ovsl(p)					\
	rcu_dereference_check(p, lockdep_ovsl_is_held())
static inline struct net *ovs_dp_get_net(const struct datapath *dp)
{
	return read_pnet(&dp->net);
}
static inline void ovs_dp_set_net(struct datapath *dp, struct net *net)
{
	write_pnet(&dp->net, net);
}
struct vport *ovs_lookup_vport(const struct datapath *dp, u16 port_no);
static inline struct vport *ovs_vport_rcu(const struct datapath *dp, int port_no)
{
	WARN_ON_ONCE(!rcu_read_lock_held());
	return ovs_lookup_vport(dp, port_no);
}
static inline struct vport *ovs_vport_ovsl_rcu(const struct datapath *dp, int port_no)
{
	WARN_ON_ONCE(!rcu_read_lock_held() && !lockdep_ovsl_is_held());
	return ovs_lookup_vport(dp, port_no);
}
static inline struct vport *ovs_vport_ovsl(const struct datapath *dp, int port_no)
{
	ASSERT_OVSL();
	return ovs_lookup_vport(dp, port_no);
}
static inline struct datapath *get_dp_rcu(struct net *net, int dp_ifindex)
{
	struct net_device *dev = dev_get_by_index_rcu(net, dp_ifindex);
	if (dev) {
		struct vport *vport = ovs_internal_dev_get_vport(dev);
		if (vport)
			return vport->dp;
	}
	return NULL;
}
static inline struct datapath *get_dp(struct net *net, int dp_ifindex)
{
	struct datapath *dp;
	WARN_ON_ONCE(!rcu_read_lock_held() && !lockdep_ovsl_is_held());
	rcu_read_lock();
	dp = get_dp_rcu(net, dp_ifindex);
	rcu_read_unlock();
	return dp;
}
extern struct notifier_block ovs_dp_device_notifier;
extern struct genl_family dp_vport_genl_family;
void ovs_dp_process_packet(struct sk_buff *skb, struct sw_flow_key *key);
void ovs_dp_detach_port(struct vport *);
int ovs_dp_upcall(struct datapath *, struct sk_buff *,
		  const struct sw_flow_key *, const struct dp_upcall_info *,
		  uint32_t cutlen);
u32 ovs_dp_get_upcall_portid(const struct datapath *dp, uint32_t cpu_id);
const char *ovs_dp_name(const struct datapath *dp);
struct sk_buff *ovs_vport_cmd_build_info(struct vport *vport, struct net *net,
					 u32 portid, u32 seq, u8 cmd);
int ovs_execute_actions(struct datapath *dp, struct sk_buff *skb,
			const struct sw_flow_actions *, struct sw_flow_key *);
void ovs_dp_notify_wq(struct work_struct *work);
int action_fifos_init(void);
void action_fifos_exit(void);
#define OVS_MASKED(OLD, KEY, MASK) ((KEY) | ((OLD) & ~(MASK)))
#define OVS_SET_MASKED(OLD, KEY, MASK) ((OLD) = OVS_MASKED(OLD, KEY, MASK))
#define OVS_NLERR(logging_allowed, fmt, ...)			\
do {								\
	if (logging_allowed && net_ratelimit())			\
		pr_info("netlink: " fmt "\n", ##__VA_ARGS__);	\
} while (0)
#endif  
