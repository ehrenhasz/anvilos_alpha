 

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/export.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/sock.h>
#include <rdma/rdma_netlink.h>
#include <linux/module.h>
#include "core_priv.h"

static struct {
	const struct rdma_nl_cbs *cb_table;
	 
	struct rw_semaphore sem;
} rdma_nl_types[RDMA_NL_NUM_CLIENTS];

bool rdma_nl_chk_listeners(unsigned int group)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(&init_net);

	return netlink_has_listeners(rnet->nl_sock, group);
}
EXPORT_SYMBOL(rdma_nl_chk_listeners);

static bool is_nl_msg_valid(unsigned int type, unsigned int op)
{
	static const unsigned int max_num_ops[RDMA_NL_NUM_CLIENTS] = {
		[RDMA_NL_IWCM] = RDMA_NL_IWPM_NUM_OPS,
		[RDMA_NL_LS] = RDMA_NL_LS_NUM_OPS,
		[RDMA_NL_NLDEV] = RDMA_NLDEV_NUM_OPS,
	};

	 
	BUILD_BUG_ON(RDMA_NL_NUM_CLIENTS != 6);

	if (type >= RDMA_NL_NUM_CLIENTS)
		return false;

	return op < max_num_ops[type];
}

static const struct rdma_nl_cbs *
get_cb_table(const struct sk_buff *skb, unsigned int type, unsigned int op)
{
	const struct rdma_nl_cbs *cb_table;

	 
	if (sock_net(skb->sk) != &init_net && type != RDMA_NL_NLDEV)
		return NULL;

	cb_table = READ_ONCE(rdma_nl_types[type].cb_table);
	if (!cb_table) {
		 
		up_read(&rdma_nl_types[type].sem);

		request_module("rdma-netlink-subsys-%u", type);

		down_read(&rdma_nl_types[type].sem);
		cb_table = READ_ONCE(rdma_nl_types[type].cb_table);
	}
	if (!cb_table || (!cb_table[op].dump && !cb_table[op].doit))
		return NULL;
	return cb_table;
}

void rdma_nl_register(unsigned int index,
		      const struct rdma_nl_cbs cb_table[])
{
	if (WARN_ON(!is_nl_msg_valid(index, 0)) ||
	    WARN_ON(READ_ONCE(rdma_nl_types[index].cb_table)))
		return;

	 
	smp_store_release(&rdma_nl_types[index].cb_table, cb_table);
}
EXPORT_SYMBOL(rdma_nl_register);

void rdma_nl_unregister(unsigned int index)
{
	down_write(&rdma_nl_types[index].sem);
	rdma_nl_types[index].cb_table = NULL;
	up_write(&rdma_nl_types[index].sem);
}
EXPORT_SYMBOL(rdma_nl_unregister);

void *ibnl_put_msg(struct sk_buff *skb, struct nlmsghdr **nlh, int seq,
		   int len, int client, int op, int flags)
{
	*nlh = nlmsg_put(skb, 0, seq, RDMA_NL_GET_TYPE(client, op), len, flags);
	if (!*nlh)
		return NULL;
	return nlmsg_data(*nlh);
}
EXPORT_SYMBOL(ibnl_put_msg);

int ibnl_put_attr(struct sk_buff *skb, struct nlmsghdr *nlh,
		  int len, void *data, int type)
{
	if (nla_put(skb, type, len, data)) {
		nlmsg_cancel(skb, nlh);
		return -EMSGSIZE;
	}
	return 0;
}
EXPORT_SYMBOL(ibnl_put_attr);

static int rdma_nl_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh,
			   struct netlink_ext_ack *extack)
{
	int type = nlh->nlmsg_type;
	unsigned int index = RDMA_NL_GET_CLIENT(type);
	unsigned int op = RDMA_NL_GET_OP(type);
	const struct rdma_nl_cbs *cb_table;
	int err = -EINVAL;

	if (!is_nl_msg_valid(index, op))
		return -EINVAL;

	down_read(&rdma_nl_types[index].sem);
	cb_table = get_cb_table(skb, index, op);
	if (!cb_table)
		goto done;

	if ((cb_table[op].flags & RDMA_NL_ADMIN_PERM) &&
	    !netlink_capable(skb, CAP_NET_ADMIN)) {
		err = -EPERM;
		goto done;
	}

	 
	if (index == RDMA_NL_LS) {
		if (cb_table[op].doit)
			err = cb_table[op].doit(skb, nlh, extack);
		goto done;
	}
	 
	if ((nlh->nlmsg_flags & NLM_F_DUMP) || index == RDMA_NL_IWCM) {
		struct netlink_dump_control c = {
			.dump = cb_table[op].dump,
		};
		if (c.dump)
			err = netlink_dump_start(skb->sk, skb, nlh, &c);
		goto done;
	}

	if (cb_table[op].doit)
		err = cb_table[op].doit(skb, nlh, extack);
done:
	up_read(&rdma_nl_types[index].sem);
	return err;
}

 
static int rdma_nl_rcv_skb(struct sk_buff *skb, int (*cb)(struct sk_buff *,
						   struct nlmsghdr *,
						   struct netlink_ext_ack *))
{
	struct netlink_ext_ack extack = {};
	struct nlmsghdr *nlh;
	int err;

	while (skb->len >= nlmsg_total_size(0)) {
		int msglen;

		nlh = nlmsg_hdr(skb);
		err = 0;

		if (nlh->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh->nlmsg_len)
			return 0;

		 
		if (!(nlh->nlmsg_flags & NLM_F_REQUEST) &&
		    (RDMA_NL_GET_CLIENT(nlh->nlmsg_type) != RDMA_NL_LS))
			goto ack;

		 
		if (nlh->nlmsg_type < NLMSG_MIN_TYPE)
			goto ack;

		err = cb(skb, nlh, &extack);
		if (err == -EINTR)
			goto skip;

ack:
		if (nlh->nlmsg_flags & NLM_F_ACK || err)
			netlink_ack(skb, nlh, err, &extack);

skip:
		msglen = NLMSG_ALIGN(nlh->nlmsg_len);
		if (msglen > skb->len)
			msglen = skb->len;
		skb_pull(skb, msglen);
	}

	return 0;
}

static void rdma_nl_rcv(struct sk_buff *skb)
{
	rdma_nl_rcv_skb(skb, &rdma_nl_rcv_msg);
}

int rdma_nl_unicast(struct net *net, struct sk_buff *skb, u32 pid)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(net);
	int err;

	err = netlink_unicast(rnet->nl_sock, skb, pid, MSG_DONTWAIT);
	return (err < 0) ? err : 0;
}
EXPORT_SYMBOL(rdma_nl_unicast);

int rdma_nl_unicast_wait(struct net *net, struct sk_buff *skb, __u32 pid)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(net);
	int err;

	err = netlink_unicast(rnet->nl_sock, skb, pid, 0);
	return (err < 0) ? err : 0;
}
EXPORT_SYMBOL(rdma_nl_unicast_wait);

int rdma_nl_multicast(struct net *net, struct sk_buff *skb,
		      unsigned int group, gfp_t flags)
{
	struct rdma_dev_net *rnet = rdma_net_to_dev_net(net);

	return nlmsg_multicast(rnet->nl_sock, skb, 0, group, flags);
}
EXPORT_SYMBOL(rdma_nl_multicast);

void rdma_nl_init(void)
{
	int idx;

	for (idx = 0; idx < RDMA_NL_NUM_CLIENTS; idx++)
		init_rwsem(&rdma_nl_types[idx].sem);
}

void rdma_nl_exit(void)
{
	int idx;

	for (idx = 0; idx < RDMA_NL_NUM_CLIENTS; idx++)
		WARN(rdma_nl_types[idx].cb_table,
		     "Netlink client %d wasn't released prior to unloading %s\n",
		     idx, KBUILD_MODNAME);
}

int rdma_nl_net_init(struct rdma_dev_net *rnet)
{
	struct net *net = read_pnet(&rnet->net);
	struct netlink_kernel_cfg cfg = {
		.input	= rdma_nl_rcv,
	};
	struct sock *nls;

	nls = netlink_kernel_create(net, NETLINK_RDMA, &cfg);
	if (!nls)
		return -ENOMEM;

	nls->sk_sndtimeo = 10 * HZ;
	rnet->nl_sock = nls;
	return 0;
}

void rdma_nl_net_exit(struct rdma_dev_net *rnet)
{
	netlink_kernel_release(rnet->nl_sock);
}

MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_RDMA);
