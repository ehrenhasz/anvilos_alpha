 

#include "addr.h"
#include "core.h"

bool tipc_in_scope(bool legacy_format, u32 domain, u32 addr)
{
	if (!domain || (domain == addr))
		return true;
	if (!legacy_format)
		return false;
	if (domain == tipc_cluster_mask(addr))  
		return true;
	if (domain == (addr & TIPC_ZONE_CLUSTER_MASK))  
		return true;
	if (domain == (addr & TIPC_ZONE_MASK))  
		return true;
	return false;
}

void tipc_set_node_id(struct net *net, u8 *id)
{
	struct tipc_net *tn = tipc_net(net);

	memcpy(tn->node_id, id, NODE_ID_LEN);
	tipc_nodeid2string(tn->node_id_string, id);
	tn->trial_addr = hash128to32(id);
	pr_info("Node identity %s, cluster identity %u\n",
		tipc_own_id_string(net), tn->net_id);
}

void tipc_set_node_addr(struct net *net, u32 addr)
{
	struct tipc_net *tn = tipc_net(net);
	u8 node_id[NODE_ID_LEN] = {0,};

	tn->node_addr = addr;
	if (!tipc_own_id(net)) {
		sprintf(node_id, "%x", addr);
		tipc_set_node_id(net, node_id);
	}
	tn->trial_addr = addr;
	tn->addr_trial_end = jiffies;
	pr_info("Node number set to %u\n", addr);
}

char *tipc_nodeid2string(char *str, u8 *id)
{
	int i;
	u8 c;

	 
	for (i = 0; i < NODE_ID_LEN; i++) {
		c = id[i];
		if (c >= '0' && c <= '9')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= 'a' && c <= 'z')
			continue;
		if (c == '.')
			continue;
		if (c == ':')
			continue;
		if (c == '_')
			continue;
		if (c == '-')
			continue;
		if (c == '@')
			continue;
		if (c != 0)
			break;
	}
	if (i == NODE_ID_LEN) {
		memcpy(str, id, NODE_ID_LEN);
		str[NODE_ID_LEN] = 0;
		return str;
	}

	 
	for (i = 0; i < NODE_ID_LEN; i++)
		sprintf(&str[2 * i], "%02x", id[i]);

	 
	for (i = NODE_ID_STR_LEN - 2; str[i] == '0'; i--)
		str[i] = 0;

	return str;
}
