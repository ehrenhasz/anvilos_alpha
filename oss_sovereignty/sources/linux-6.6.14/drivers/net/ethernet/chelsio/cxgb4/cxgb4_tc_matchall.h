


#ifndef __CXGB4_TC_MATCHALL_H__
#define __CXGB4_TC_MATCHALL_H__

#include <net/pkt_cls.h>

enum cxgb4_matchall_state {
	CXGB4_MATCHALL_STATE_DISABLED = 0,
	CXGB4_MATCHALL_STATE_ENABLED,
};

struct cxgb4_matchall_egress_entry {
	enum cxgb4_matchall_state state; 
	u8 hwtc; 
	u64 cookie; 
};

struct cxgb4_matchall_ingress_entry {
	enum cxgb4_matchall_state state; 
	u32 tid[CXGB4_FILTER_TYPE_MAX]; 
	
	struct ch_filter_specification fs[CXGB4_FILTER_TYPE_MAX];
	u16 viid_mirror; 
	u64 bytes; 
	u64 packets; 
	u64 last_used; 
};

struct cxgb4_tc_port_matchall {
	struct cxgb4_matchall_egress_entry egress; 
	struct cxgb4_matchall_ingress_entry ingress; 
};

struct cxgb4_tc_matchall {
	struct cxgb4_tc_port_matchall *port_matchall; 
};

int cxgb4_tc_matchall_replace(struct net_device *dev,
			      struct tc_cls_matchall_offload *cls_matchall,
			      bool ingress);
int cxgb4_tc_matchall_destroy(struct net_device *dev,
			      struct tc_cls_matchall_offload *cls_matchall,
			      bool ingress);
int cxgb4_tc_matchall_stats(struct net_device *dev,
			    struct tc_cls_matchall_offload *cls_matchall);

int cxgb4_init_tc_matchall(struct adapter *adap);
void cxgb4_cleanup_tc_matchall(struct adapter *adap);
#endif 
