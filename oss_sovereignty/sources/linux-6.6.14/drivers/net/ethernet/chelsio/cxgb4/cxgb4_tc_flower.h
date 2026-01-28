

#ifndef __CXGB4_TC_FLOWER_H
#define __CXGB4_TC_FLOWER_H

#include <net/pkt_cls.h>

struct ch_tc_flower_stats {
	u64 prev_packet_count;
	u64 packet_count;
	u64 byte_count;
	u64 last_used;
};

struct ch_tc_flower_entry {
	struct ch_filter_specification fs;
	struct ch_tc_flower_stats stats;
	unsigned long tc_flower_cookie;
	struct rhash_head node;
	struct rcu_head rcu;
	spinlock_t lock; 
	u32 filter_id;
};

enum {
	ETH_DMAC_31_0,	
	ETH_DMAC_47_32,	
	ETH_SMAC_15_0,	
	ETH_SMAC_47_16,	

	IP4_SRC,	
	IP4_DST,	

	IP6_SRC_31_0,	
	IP6_SRC_63_32,	
	IP6_SRC_95_64,	
	IP6_SRC_127_96,	

	IP6_DST_31_0,	
	IP6_DST_63_32,	
	IP6_DST_95_64,	
	IP6_DST_127_96,	

	TCP_SPORT,	
	TCP_DPORT,	

	UDP_SPORT,	
	UDP_DPORT,	
};

struct ch_tc_pedit_fields {
	u8 field;
	u8 size;
	u32 offset;
};

#define PEDIT_FIELDS(type, field, size, fs_field, offset) \
	{ type## field, size, \
		offsetof(struct ch_filter_specification, fs_field) + (offset) }

#define PEDIT_ETH_DMAC_MASK		0xffff
#define PEDIT_TCP_UDP_SPORT_MASK	0xffff
#define PEDIT_ETH_DMAC_31_0		0x0
#define PEDIT_ETH_DMAC_47_32_SMAC_15_0	0x4
#define PEDIT_ETH_SMAC_47_16		0x8
#define PEDIT_IP4_SRC			0xC
#define PEDIT_IP4_DST			0x10
#define PEDIT_IP6_SRC_31_0		0x8
#define PEDIT_IP6_SRC_63_32		0xC
#define PEDIT_IP6_SRC_95_64		0x10
#define PEDIT_IP6_SRC_127_96		0x14
#define PEDIT_IP6_DST_31_0		0x18
#define PEDIT_IP6_DST_63_32		0x1C
#define PEDIT_IP6_DST_95_64		0x20
#define PEDIT_IP6_DST_127_96		0x24
#define PEDIT_TCP_SPORT_DPORT		0x0
#define PEDIT_UDP_SPORT_DPORT		0x0

enum cxgb4_action_natmode_flags {
	CXGB4_ACTION_NATMODE_NONE = 0,
	CXGB4_ACTION_NATMODE_DIP = (1 << 0),
	CXGB4_ACTION_NATMODE_SIP = (1 << 1),
	CXGB4_ACTION_NATMODE_DPORT = (1 << 2),
	CXGB4_ACTION_NATMODE_SPORT = (1 << 3),
};


struct cxgb4_natmode_config {
	enum chip_type chip;
	u8 flags;
	u8 natmode;
};

void cxgb4_process_flow_actions(struct net_device *in,
				struct flow_action *actions,
				struct ch_filter_specification *fs);
int cxgb4_validate_flow_actions(struct net_device *dev,
				struct flow_action *actions,
				struct netlink_ext_ack *extack,
				u8 matchall_filter);

int cxgb4_tc_flower_replace(struct net_device *dev,
			    struct flow_cls_offload *cls);
int cxgb4_tc_flower_destroy(struct net_device *dev,
			    struct flow_cls_offload *cls);
int cxgb4_tc_flower_stats(struct net_device *dev,
			  struct flow_cls_offload *cls);
int cxgb4_flow_rule_replace(struct net_device *dev, struct flow_rule *rule,
			    u32 tc_prio, struct netlink_ext_ack *extack,
			    struct ch_filter_specification *fs, u32 *tid);
int cxgb4_flow_rule_destroy(struct net_device *dev, u32 tc_prio,
			    struct ch_filter_specification *fs, int tid);

int cxgb4_init_tc_flower(struct adapter *adap);
void cxgb4_cleanup_tc_flower(struct adapter *adap);
#endif 
