 

#include "core.h"
#include "net.h"
#include "name_distr.h"
#include "subscr.h"
#include "socket.h"
#include "node.h"
#include "bcast.h"
#include "link.h"
#include "netlink.h"
#include "monitor.h"

 

static void tipc_net_finalize(struct net *net, u32 addr);

int tipc_net_init(struct net *net, u8 *node_id, u32 addr)
{
	if (tipc_own_id(net)) {
		pr_info("Cannot configure node identity twice\n");
		return -1;
	}
	pr_info("Started in network mode\n");

	if (node_id)
		tipc_set_node_id(net, node_id);
	if (addr)
		tipc_net_finalize(net, addr);
	return 0;
}

static void tipc_net_finalize(struct net *net, u32 addr)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_socket_addr sk = {0, addr};
	struct tipc_uaddr ua;

	tipc_uaddr(&ua, TIPC_SERVICE_RANGE, TIPC_CLUSTER_SCOPE,
		   TIPC_NODE_STATE, addr, addr);

	if (cmpxchg(&tn->node_addr, 0, addr))
		return;
	tipc_set_node_addr(net, addr);
	tipc_named_reinit(net);
	tipc_sk_reinit(net);
	tipc_mon_reinit_self(net);
	tipc_nametbl_publish(net, &ua, &sk, addr);
}

void tipc_net_finalize_work(struct work_struct *work)
{
	struct tipc_net *tn = container_of(work, struct tipc_net, work);

	tipc_net_finalize(tipc_link_net(tn->bcl), tn->trial_addr);
}

void tipc_net_stop(struct net *net)
{
	if (!tipc_own_id(net))
		return;

	rtnl_lock();
	tipc_bearer_stop(net);
	tipc_node_stop(net);
	rtnl_unlock();

	pr_info("Left network mode\n");
}

static int __tipc_nl_add_net(struct net *net, struct tipc_nl_msg *msg)
{
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	u64 *w0 = (u64 *)&tn->node_id[0];
	u64 *w1 = (u64 *)&tn->node_id[8];
	struct nlattr *attrs;
	void *hdr;

	hdr = genlmsg_put(msg->skb, msg->portid, msg->seq, &tipc_genl_family,
			  NLM_F_MULTI, TIPC_NL_NET_GET);
	if (!hdr)
		return -EMSGSIZE;

	attrs = nla_nest_start_noflag(msg->skb, TIPC_NLA_NET);
	if (!attrs)
		goto msg_full;

	if (nla_put_u32(msg->skb, TIPC_NLA_NET_ID, tn->net_id))
		goto attr_msg_full;
	if (nla_put_u64_64bit(msg->skb, TIPC_NLA_NET_NODEID, *w0, 0))
		goto attr_msg_full;
	if (nla_put_u64_64bit(msg->skb, TIPC_NLA_NET_NODEID_W1, *w1, 0))
		goto attr_msg_full;
	nla_nest_end(msg->skb, attrs);
	genlmsg_end(msg->skb, hdr);

	return 0;

attr_msg_full:
	nla_nest_cancel(msg->skb, attrs);
msg_full:
	genlmsg_cancel(msg->skb, hdr);

	return -EMSGSIZE;
}

int tipc_nl_net_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	int err;
	int done = cb->args[0];
	struct tipc_nl_msg msg;

	if (done)
		return 0;

	msg.skb = skb;
	msg.portid = NETLINK_CB(cb->skb).portid;
	msg.seq = cb->nlh->nlmsg_seq;

	err = __tipc_nl_add_net(net, &msg);
	if (err)
		goto out;

	done = 1;
out:
	cb->args[0] = done;

	return skb->len;
}

int __tipc_nl_net_set(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[TIPC_NLA_NET_MAX + 1];
	struct net *net = sock_net(skb->sk);
	struct tipc_net *tn = tipc_net(net);
	int err;

	if (!info->attrs[TIPC_NLA_NET])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_NET_MAX,
					  info->attrs[TIPC_NLA_NET],
					  tipc_nl_net_policy, info->extack);

	if (err)
		return err;

	 
	if (tipc_own_addr(net))
		return -EPERM;

	if (attrs[TIPC_NLA_NET_ID]) {
		u32 val;

		val = nla_get_u32(attrs[TIPC_NLA_NET_ID]);
		if (val < 1 || val > 9999)
			return -EINVAL;

		tn->net_id = val;
	}

	if (attrs[TIPC_NLA_NET_ADDR]) {
		u32 addr;

		addr = nla_get_u32(attrs[TIPC_NLA_NET_ADDR]);
		if (!addr)
			return -EINVAL;
		tn->legacy_addr_format = true;
		tipc_net_init(net, NULL, addr);
	}

	if (attrs[TIPC_NLA_NET_NODEID]) {
		u8 node_id[NODE_ID_LEN];
		u64 *w0 = (u64 *)&node_id[0];
		u64 *w1 = (u64 *)&node_id[8];

		if (!attrs[TIPC_NLA_NET_NODEID_W1])
			return -EINVAL;
		*w0 = nla_get_u64(attrs[TIPC_NLA_NET_NODEID]);
		*w1 = nla_get_u64(attrs[TIPC_NLA_NET_NODEID_W1]);
		tipc_net_init(net, node_id, 0);
	}
	return 0;
}

int tipc_nl_net_set(struct sk_buff *skb, struct genl_info *info)
{
	int err;

	rtnl_lock();
	err = __tipc_nl_net_set(skb, info);
	rtnl_unlock();

	return err;
}

static int __tipc_nl_addr_legacy_get(struct net *net, struct tipc_nl_msg *msg)
{
	struct tipc_net *tn = tipc_net(net);
	struct nlattr *attrs;
	void *hdr;

	hdr = genlmsg_put(msg->skb, msg->portid, msg->seq, &tipc_genl_family,
			  0, TIPC_NL_ADDR_LEGACY_GET);
	if (!hdr)
		return -EMSGSIZE;

	attrs = nla_nest_start(msg->skb, TIPC_NLA_NET);
	if (!attrs)
		goto msg_full;

	if (tn->legacy_addr_format)
		if (nla_put_flag(msg->skb, TIPC_NLA_NET_ADDR_LEGACY))
			goto attr_msg_full;

	nla_nest_end(msg->skb, attrs);
	genlmsg_end(msg->skb, hdr);

	return 0;

attr_msg_full:
	nla_nest_cancel(msg->skb, attrs);
msg_full:
	genlmsg_cancel(msg->skb, hdr);

	return -EMSGSIZE;
}

int tipc_nl_net_addr_legacy_get(struct sk_buff *skb, struct genl_info *info)
{
	struct net *net = sock_net(skb->sk);
	struct tipc_nl_msg msg;
	struct sk_buff *rep;
	int err;

	rep = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!rep)
		return -ENOMEM;

	msg.skb = rep;
	msg.portid = info->snd_portid;
	msg.seq = info->snd_seq;

	err = __tipc_nl_addr_legacy_get(net, &msg);
	if (err) {
		nlmsg_free(msg.skb);
		return err;
	}

	return genlmsg_reply(msg.skb, info);
}
