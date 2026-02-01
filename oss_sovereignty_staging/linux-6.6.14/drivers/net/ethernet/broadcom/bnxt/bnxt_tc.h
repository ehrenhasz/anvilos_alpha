 

#ifndef BNXT_TC_H
#define BNXT_TC_H

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD

#include <net/ip_tunnels.h>

 
struct bnxt_tc_l2_key {
	u8		dmac[ETH_ALEN];
	u8		smac[ETH_ALEN];
	__be16		inner_vlan_tpid;
	__be16		inner_vlan_tci;
	__be16		ether_type;
	u8		num_vlans;
	u8		dir;
#define BNXT_DIR_RX	1
#define BNXT_DIR_TX	0
};

struct bnxt_tc_l3_key {
	union {
		struct {
			struct in_addr daddr;
			struct in_addr saddr;
		} ipv4;
		struct {
			struct in6_addr daddr;
			struct in6_addr saddr;
		} ipv6;
	};
};

struct bnxt_tc_l4_key {
	u8  ip_proto;
	union {
		struct {
			__be16 sport;
			__be16 dport;
		} ports;
		struct {
			u8 type;
			u8 code;
		} icmp;
	};
};

struct bnxt_tc_tunnel_key {
	struct bnxt_tc_l2_key	l2;
	struct bnxt_tc_l3_key	l3;
	struct bnxt_tc_l4_key	l4;
	__be32			id;
};

#define bnxt_eth_addr_key_mask_invalid(eth_addr, eth_addr_mask)		\
	((is_wildcard(&(eth_addr)[0], ETH_ALEN) &&			\
	 is_wildcard(&(eth_addr)[ETH_ALEN / 2], ETH_ALEN)) ||		\
	(is_wildcard(&(eth_addr_mask)[0], ETH_ALEN) &&			\
	 is_wildcard(&(eth_addr_mask)[ETH_ALEN / 2], ETH_ALEN)))

struct bnxt_tc_actions {
	u32				flags;
#define BNXT_TC_ACTION_FLAG_FWD			BIT(0)
#define BNXT_TC_ACTION_FLAG_FWD_VXLAN		BIT(1)
#define BNXT_TC_ACTION_FLAG_PUSH_VLAN		BIT(3)
#define BNXT_TC_ACTION_FLAG_POP_VLAN		BIT(4)
#define BNXT_TC_ACTION_FLAG_DROP		BIT(5)
#define BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP	BIT(6)
#define BNXT_TC_ACTION_FLAG_TUNNEL_DECAP	BIT(7)
#define BNXT_TC_ACTION_FLAG_L2_REWRITE		BIT(8)
#define BNXT_TC_ACTION_FLAG_NAT_XLATE		BIT(9)

	u16				dst_fid;
	struct net_device		*dst_dev;
	__be16				push_vlan_tpid;
	__be16				push_vlan_tci;

	 
	struct ip_tunnel_key		tun_encap_key;
#define	PEDIT_OFFSET_SMAC_LAST_4_BYTES		0x8
	__be16				l2_rewrite_dmac[3];
	__be16				l2_rewrite_smac[3];
	struct {
		bool src_xlate;   
		bool l3_is_ipv4;  
		struct bnxt_tc_l3_key l3;
		struct bnxt_tc_l4_key l4;
	} nat;
};

struct bnxt_tc_flow {
	u32				flags;
#define BNXT_TC_FLOW_FLAGS_ETH_ADDRS		BIT(1)
#define BNXT_TC_FLOW_FLAGS_IPV4_ADDRS		BIT(2)
#define BNXT_TC_FLOW_FLAGS_IPV6_ADDRS		BIT(3)
#define BNXT_TC_FLOW_FLAGS_PORTS		BIT(4)
#define BNXT_TC_FLOW_FLAGS_ICMP			BIT(5)
#define BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS	BIT(6)
#define BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS	BIT(7)
#define BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS	BIT(8)
#define BNXT_TC_FLOW_FLAGS_TUNL_PORTS		BIT(9)
#define BNXT_TC_FLOW_FLAGS_TUNL_ID		BIT(10)
#define BNXT_TC_FLOW_FLAGS_TUNNEL	(BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS | \
					 BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS | \
					 BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS |\
					 BNXT_TC_FLOW_FLAGS_TUNL_PORTS |\
					 BNXT_TC_FLOW_FLAGS_TUNL_ID)

	 
	u16				src_fid;
	struct bnxt_tc_l2_key		l2_key;
	struct bnxt_tc_l2_key		l2_mask;
	struct bnxt_tc_l3_key		l3_key;
	struct bnxt_tc_l3_key		l3_mask;
	struct bnxt_tc_l4_key		l4_key;
	struct bnxt_tc_l4_key		l4_mask;
	struct ip_tunnel_key		tun_key;
	struct ip_tunnel_key		tun_mask;

	struct bnxt_tc_actions		actions;

	 
	struct bnxt_tc_flow_stats	stats;
	 
	struct bnxt_tc_flow_stats	prev_stats;
	unsigned long			lastused;  
	 
	spinlock_t			stats_lock;
};

 
struct bnxt_tc_tunnel_node {
	struct ip_tunnel_key		key;
	struct rhash_head		node;

	 
	struct bnxt_tc_l2_key		l2_info;

#define	INVALID_TUNNEL_HANDLE		cpu_to_le32(0xffffffff)
	 
	__le32				tunnel_handle;

	u32				refcount;
	struct rcu_head			rcu;
};

 
struct bnxt_tc_l2_node {
	 
#define BNXT_TC_L2_KEY_LEN			16
	struct bnxt_tc_l2_key	key;
	struct rhash_head	node;

	 
	struct list_head	common_l2_flows;

	 
	u16			refcount;

	struct rcu_head		rcu;
};

struct bnxt_tc_flow_node {
	 
	unsigned long			cookie;
	struct rhash_head		node;

	struct bnxt_tc_flow		flow;

	__le64				ext_flow_handle;
	__le16				flow_handle;
	__le32				flow_id;

	 
	struct bnxt_tc_l2_node		*l2_node;
	 
	struct list_head		l2_list_node;

	 
	struct bnxt_tc_tunnel_node	*encap_node;

	 
	struct bnxt_tc_tunnel_node	*decap_node;
	 
	struct bnxt_tc_l2_node		*decap_l2_node;
	 
	struct list_head		decap_l2_list_node;

	struct rcu_head			rcu;
};

int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *cls_flower);
int bnxt_init_tc(struct bnxt *bp);
void bnxt_shutdown_tc(struct bnxt *bp);
void bnxt_tc_flow_stats_work(struct bnxt *bp);

static inline bool bnxt_tc_flower_enabled(struct bnxt *bp)
{
	return bp->tc_info && bp->tc_info->enabled;
}

#else  

static inline int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
				       struct flow_cls_offload *cls_flower)
{
	return -EOPNOTSUPP;
}

static inline int bnxt_init_tc(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_shutdown_tc(struct bnxt *bp)
{
}

static inline void bnxt_tc_flow_stats_work(struct bnxt *bp)
{
}

static inline bool bnxt_tc_flower_enabled(struct bnxt *bp)
{
	return false;
}
#endif  
#endif  
