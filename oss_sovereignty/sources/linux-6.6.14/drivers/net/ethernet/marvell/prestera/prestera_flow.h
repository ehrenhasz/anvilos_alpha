


#ifndef _PRESTERA_FLOW_H_
#define _PRESTERA_FLOW_H_

#include <net/flow_offload.h>

struct prestera_port;
struct prestera_switch;

struct prestera_flow_block_binding {
	struct list_head list;
	struct prestera_port *port;
	int span_id;
};

struct prestera_flow_block {
	struct list_head binding_list;
	struct prestera_switch *sw;
	struct net *net;
	struct prestera_acl_ruleset *ruleset_zero;
	struct flow_block_cb *block_cb;
	struct list_head template_list;
	struct {
		u32 prio_min;
		u32 prio_max;
		bool bound;
	} mall;
	unsigned int rule_count;
	bool ingress;
};

int prestera_flow_block_setup(struct prestera_port *port,
			      struct flow_block_offload *f);

#endif 
