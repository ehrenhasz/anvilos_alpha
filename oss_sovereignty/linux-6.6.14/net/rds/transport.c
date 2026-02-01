 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/ipv6.h>

#include "rds.h"
#include "loop.h"

static char * const rds_trans_modules[] = {
	[RDS_TRANS_IB] = "rds_rdma",
	[RDS_TRANS_GAP] = NULL,
	[RDS_TRANS_TCP] = "rds_tcp",
};

static struct rds_transport *transports[RDS_TRANS_COUNT];
static DECLARE_RWSEM(rds_trans_sem);

void rds_trans_register(struct rds_transport *trans)
{
	BUG_ON(strlen(trans->t_name) + 1 > TRANSNAMSIZ);

	down_write(&rds_trans_sem);

	if (transports[trans->t_type])
		printk(KERN_ERR "RDS Transport type %d already registered\n",
			trans->t_type);
	else {
		transports[trans->t_type] = trans;
		printk(KERN_INFO "Registered RDS/%s transport\n", trans->t_name);
	}

	up_write(&rds_trans_sem);
}
EXPORT_SYMBOL_GPL(rds_trans_register);

void rds_trans_unregister(struct rds_transport *trans)
{
	down_write(&rds_trans_sem);

	transports[trans->t_type] = NULL;
	printk(KERN_INFO "Unregistered RDS/%s transport\n", trans->t_name);

	up_write(&rds_trans_sem);
}
EXPORT_SYMBOL_GPL(rds_trans_unregister);

void rds_trans_put(struct rds_transport *trans)
{
	if (trans)
		module_put(trans->t_owner);
}

struct rds_transport *rds_trans_get_preferred(struct net *net,
					      const struct in6_addr *addr,
					      __u32 scope_id)
{
	struct rds_transport *ret = NULL;
	struct rds_transport *trans;
	unsigned int i;

	if (ipv6_addr_v4mapped(addr)) {
		if (*(u_int8_t *)&addr->s6_addr32[3] == IN_LOOPBACKNET)
			return &rds_loop_transport;
	} else if (ipv6_addr_loopback(addr)) {
		return &rds_loop_transport;
	}

	down_read(&rds_trans_sem);
	for (i = 0; i < RDS_TRANS_COUNT; i++) {
		trans = transports[i];

		if (trans && (trans->laddr_check(net, addr, scope_id) == 0) &&
		    (!trans->t_owner || try_module_get(trans->t_owner))) {
			ret = trans;
			break;
		}
	}
	up_read(&rds_trans_sem);

	return ret;
}

struct rds_transport *rds_trans_get(int t_type)
{
	struct rds_transport *ret = NULL;
	struct rds_transport *trans;

	down_read(&rds_trans_sem);
	trans = transports[t_type];
	if (!trans) {
		up_read(&rds_trans_sem);
		if (rds_trans_modules[t_type])
			request_module(rds_trans_modules[t_type]);
		down_read(&rds_trans_sem);
		trans = transports[t_type];
	}
	if (trans && trans->t_type == t_type &&
	    (!trans->t_owner || try_module_get(trans->t_owner)))
		ret = trans;

	up_read(&rds_trans_sem);

	return ret;
}

 
unsigned int rds_trans_stats_info_copy(struct rds_info_iterator *iter,
				       unsigned int avail)

{
	struct rds_transport *trans;
	unsigned int total = 0;
	unsigned int part;
	int i;

	rds_info_iter_unmap(iter);
	down_read(&rds_trans_sem);

	for (i = 0; i < RDS_TRANS_COUNT; i++) {
		trans = transports[i];
		if (!trans || !trans->stats_info_copy)
			continue;

		part = trans->stats_info_copy(iter, avail);
		avail -= min(avail, part);
		total += part;
	}

	up_read(&rds_trans_sem);

	return total;
}
